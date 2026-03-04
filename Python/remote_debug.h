/*
 * Remote debugging support for CPython (PEP 768 backport to 3.12).
 *
 * This header provides Linux-specific helpers for reading/writing
 * memory of a remote process using process_vm_readv/process_vm_writev.
 *
 * IMPORTANT: All functions are static to avoid PLT entries that
 * could be used in return-oriented programming attacks.
 */

#ifndef Py_REMOTE_DEBUG_H
#define Py_REMOTE_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pyconfig.h"
#include "Python.h"
#include "internal/pycore_runtime.h"
#include "internal/pycore_debug_offsets.h"

#ifdef __linux__
#  include <sys/uio.h>
#  include <elf.h>
#  if INTPTR_MAX == INT64_MAX
#    define Elf_Ehdr Elf64_Ehdr
#    define Elf_Shdr Elf64_Shdr
#    define Elf_Phdr Elf64_Phdr
#  else
#    define Elf_Ehdr Elf32_Ehdr
#    define Elf_Shdr Elf32_Shdr
#    define Elf_Phdr Elf32_Phdr
#  endif
#endif

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Process handle for reading/writing remote process memory (Linux only) */
typedef struct {
    pid_t pid;
#ifdef __linux__
    int memfd;
#endif
} proc_handle_t;

/* Initialize the process handle */
static int
_Py_RemoteDebug_InitProcHandle(proc_handle_t *handle, pid_t pid) {
    handle->pid = pid;
#ifdef __linux__
    handle->memfd = -1;
#endif
    return 0;
}

/* Clean up the process handle */
static void
_Py_RemoteDebug_CleanupProcHandle(proc_handle_t *handle) {
#ifdef __linux__
    if (handle->memfd != -1) {
        close(handle->memfd);
        handle->memfd = -1;
    }
#endif
    handle->pid = 0;
}

#ifdef __linux__

/* Read memory from a remote process using process_vm_readv */
static int
_Py_RemoteDebug_ReadRemoteMemory(proc_handle_t *handle, uintptr_t remote_address, size_t len, void* dst)
{
    struct iovec local_iov = { .iov_base = dst, .iov_len = len };
    struct iovec remote_iov = { .iov_base = (void*)remote_address, .iov_len = len };

    ssize_t result = process_vm_readv(handle->pid, &local_iov, 1, &remote_iov, 1, 0);
    if (result == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    if ((size_t)result != len) {
        PyErr_Format(PyExc_RuntimeError,
            "Short read from remote process %d: expected %zu bytes, got %zd",
            handle->pid, len, result);
        return -1;
    }
    return 0;
}

/* Write memory to a remote process using process_vm_writev */
static int
_Py_RemoteDebug_WriteRemoteMemory(proc_handle_t *handle, uintptr_t remote_address, size_t len, const void* src)
{
    struct iovec local_iov = { .iov_base = (void*)src, .iov_len = len };
    struct iovec remote_iov = { .iov_base = (void*)remote_address, .iov_len = len };

    ssize_t result = process_vm_writev(handle->pid, &local_iov, 1, &remote_iov, 1, 0);
    if (result == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    if ((size_t)result != len) {
        PyErr_Format(PyExc_RuntimeError,
            "Short write to remote process %d: expected %zu bytes, wrote %zd",
            handle->pid, len, result);
        return -1;
    }
    return 0;
}

/*
 * Find the _PyRuntime address in a remote process by scanning /proc/pid/maps
 * for the python binary and reading the ELF sections to find the .pyruntime section.
 */
static int
_Py_RemoteDebug_ReadDebugOffsets(
    proc_handle_t *handle,
    uintptr_t *runtime_start_address,
    _Py_DebugOffsets *debug_offsets)
{
    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", handle->pid);

    FILE *maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    char line[1024];
    uintptr_t base_addr = 0;
    char binary_path[512] = {0};

    /* Find the first executable mapping that contains "python" */
    while (fgets(line, sizeof(line), maps_file)) {
        uintptr_t start, end;
        char perms[5];
        unsigned long offset;
        int dev_major, dev_minor;
        unsigned long inode;
        char path[512] = {0};

        int n = sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %511s",
                        &start, &end, perms, &offset,
                        &dev_major, &dev_minor, &inode, path);
        if (n < 7) continue;

        /* Look for the first r-x mapping of the python binary */
        if (path[0] && strstr(path, "python") && perms[0] == 'r' && offset == 0) {
            base_addr = start;
            strncpy(binary_path, path, sizeof(binary_path) - 1);
            break;
        }
    }
    fclose(maps_file);

    if (base_addr == 0 || binary_path[0] == '\0') {
        PyErr_SetString(PyExc_RuntimeError,
            "Can't find Python binary in remote process memory maps");
        return -1;
    }

    /* Open the binary file to read ELF sections */
    int fd = open(binary_path, O_RDONLY);
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    Elf_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        close(fd);
        PyErr_SetString(PyExc_RuntimeError, "Failed to read ELF header");
        return -1;
    }

    /* Read section headers */
    size_t shdr_size = ehdr.e_shnum * ehdr.e_shentsize;
    Elf_Shdr *shdrs = malloc(shdr_size);
    if (!shdrs) {
        close(fd);
        PyErr_NoMemory();
        return -1;
    }

    if (lseek(fd, ehdr.e_shoff, SEEK_SET) == -1 ||
        read(fd, shdrs, shdr_size) != (ssize_t)shdr_size) {
        free(shdrs);
        close(fd);
        PyErr_SetString(PyExc_RuntimeError, "Failed to read ELF section headers");
        return -1;
    }

    /* Read section string table */
    Elf_Shdr *shstrtab_hdr = &shdrs[ehdr.e_shstrndx];
    char *shstrtab = malloc(shstrtab_hdr->sh_size);
    if (!shstrtab) {
        free(shdrs);
        close(fd);
        PyErr_NoMemory();
        return -1;
    }

    if (lseek(fd, shstrtab_hdr->sh_offset, SEEK_SET) == -1 ||
        read(fd, shstrtab, shstrtab_hdr->sh_size) != (ssize_t)shstrtab_hdr->sh_size) {
        free(shstrtab);
        free(shdrs);
        close(fd);
        PyErr_SetString(PyExc_RuntimeError, "Failed to read section string table");
        return -1;
    }

    /* Find the .pyruntime section */
    uintptr_t pyruntime_addr = 0;
    for (int i = 0; i < ehdr.e_shnum; i++) {
        const char *name = shstrtab + shdrs[i].sh_name;
        if (strcmp(name, ".pyruntime") == 0) {
            pyruntime_addr = shdrs[i].sh_addr;
            break;
        }
    }

    free(shstrtab);
    free(shdrs);
    close(fd);

    if (pyruntime_addr == 0) {
        /* No .pyruntime section - try to find _PyRuntime symbol directly.
           Fall back to scanning for the debug cookie in the data segments. */

        /* Try reading at the base address + known offsets, or scan for cookie */
        /* For now, scan /proc/pid/maps for data segments and search for cookie */
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", handle->pid);
        maps_file = fopen(maps_path, "r");
        if (!maps_file) {
            PyErr_SetFromErrno(PyExc_OSError);
            return -1;
        }

        int found = 0;
        while (fgets(line, sizeof(line), maps_file)) {
            uintptr_t start, end;
            char perms[5];
            unsigned long offset;
            int dev_major, dev_minor;
            unsigned long inode;
            char path[512] = {0};

            int n = sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %511s",
                            &start, &end, perms, &offset,
                            &dev_major, &dev_minor, &inode, path);
            if (n < 7) continue;

            /* Look for rw- mappings of the python binary */
            if (path[0] && strstr(path, "python") && perms[0] == 'r' && perms[1] == 'w') {
                /* Scan this region for the debug cookie */
                size_t scan_step = sizeof(void*);  /* Cookie should be pointer-aligned */

                for (uintptr_t addr = start; addr < end - sizeof(_Py_Debug_Cookie); addr += scan_step) {
                    char cookie_buf[sizeof(_Py_Debug_Cookie) - 1];
                    if (_Py_RemoteDebug_ReadRemoteMemory(handle, addr, sizeof(cookie_buf), cookie_buf) != 0) {
                        PyErr_Clear();
                        continue;
                    }
                    if (memcmp(cookie_buf, _Py_Debug_Cookie, sizeof(cookie_buf)) == 0) {
                        *runtime_start_address = addr;
                        if (_Py_RemoteDebug_ReadRemoteMemory(handle, addr, sizeof(_Py_DebugOffsets), debug_offsets) != 0) {
                            fclose(maps_file);
                            return -1;
                        }
                        found = 1;
                        break;
                    }
                }
                if (found) break;
            }
        }
        fclose(maps_file);

        if (!found) {
            PyErr_SetString(PyExc_RuntimeError,
                "Can't find _PyRuntime in the remote process");
            return -1;
        }
        return 0;
    }

    /* We have a .pyruntime section address - compute runtime address */
    /* The section address is a virtual address; with PIE, add the base load address */
    /* Read program headers to find the load bias */
    fd = open(binary_path, O_RDONLY);
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    /* Re-read ELF header */
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        close(fd);
        PyErr_SetString(PyExc_RuntimeError, "Failed to re-read ELF header");
        return -1;
    }

    /* Find the first PT_LOAD segment to compute load bias */
    uintptr_t load_bias = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf_Phdr phdr;
        if (lseek(fd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) == -1) continue;
        if (read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) continue;
        if (phdr.p_type == PT_LOAD) {
            load_bias = base_addr - phdr.p_vaddr;
            break;
        }
    }
    close(fd);

    *runtime_start_address = pyruntime_addr + load_bias;

    /* Read the debug offsets from the remote process */
    if (_Py_RemoteDebug_ReadRemoteMemory(handle, *runtime_start_address,
                                         sizeof(_Py_DebugOffsets), debug_offsets) != 0) {
        return -1;
    }

    return 0;
}

#endif /* __linux__ */

#ifdef __cplusplus
}
#endif

#endif /* Py_REMOTE_DEBUG_H */
