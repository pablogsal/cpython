#include <windows.h>
#include <winternl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_THREADS 768
#define LENGTH_MISMATCH ((NTSTATUS)0xc0000004L)
static HANDLE stop_event, threads[MAX_THREADS];
static DWORD tids[MAX_THREADS];
static void *buffer;
static ULONG capacity;
static volatile ULONG sink;

static DWORD WINAPI sleeper(void *unused) {
    (void)unused;
    WaitForSingleObject(stop_event, INFINITE);
    return 0;
}

static SYSTEM_PROCESS_INFORMATION *snapshot(void) {
    ULONG required = 0;
    NTSTATUS status;
    while ((status = NtQuerySystemInformation(SystemProcessInformation,
                    buffer, capacity, &required)) == LENGTH_MISMATCH) {
        ULONG next = required + 65536;
        if (next < capacity * 2) next = capacity * 2;
        void *grown = realloc(buffer, next);
        if (!grown) exit(2);
        buffer = grown;
        capacity = next;
    }
    if (status != 0) {fprintf(stderr, "snapshot status=%lx\n", (unsigned long)status);exit(3);}
    SYSTEM_PROCESS_INFORMATION *process = buffer;
    while ((DWORD)(uintptr_t)process->UniqueProcessId != GetCurrentProcessId()) {
        if (!process->NextEntryOffset) exit(4);
        process = (void *)((char *)process + process->NextEntryOffset);
    }
    return process;
}

static ULONG state_from_snapshot(SYSTEM_PROCESS_INFORMATION *process, DWORD tid) {
    SYSTEM_THREAD_INFORMATION *info = (void *)((char *)process + sizeof(*process));
    for (ULONG i = 0; i < process->NumberOfThreads; i++) {
        if ((DWORD)(uintptr_t)info[i].ClientId.UniqueThread == tid) return info[i].ThreadState;
    }
    exit(5);
}

static ULONG direct_state(DWORD tid) {
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    if (!thread) {fprintf(stderr, "OpenThread error=%lu\n", GetLastError());exit(6);}
    SYSTEM_THREAD_INFORMATION info;
    ULONG size = 0;
    NTSTATUS status = NtQueryInformationThread(thread, (THREADINFOCLASS)40,
                                               &info, sizeof(info), &size);
    CloseHandle(thread);
    if (status || size != sizeof(info) ||
        (DWORD)(uintptr_t)info.ClientId.UniqueThread != tid ||
        (DWORD)(uintptr_t)info.ClientId.UniqueProcess != GetCurrentProcessId()) {
        fprintf(stderr, "direct status=%lx size=%lu expected=%zu\n", (unsigned long)status,size,sizeof(info));
        exit(7);
    }
    return info.ThreadState;
}

static void sample(int method, unsigned count) {
    SYSTEM_PROCESS_INFORMATION *all = method == 2 ? snapshot() : NULL;
    for (unsigned i = 0; i < count; i++) {
        ULONG state = method == 1 ? direct_state(tids[i])
            : state_from_snapshot(method == 2 ? all : snapshot(), tids[i]);
        if (state != 5) {fprintf(stderr, "Expected waiting state, got %lu\n",state);exit(8);}
        sink += state;
    }
}

int main(void) {
    const unsigned counts[] = {1, 16, 128};
    const char *names[] = {"baseline_per_thread", "direct_thread", "snapshot_per_sample"};
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    puts("pointer_bits,sampled_threads,extra_threads,round,method,samples,ns_per_sample");
    for (unsigned extra = 0; extra <= 512; extra += 512) {
        for (unsigned c = 0; c < 3; c++) {
            unsigned count = counts[c], total = count + extra;
            stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
            for (unsigned i = 0; i < total; i++) {
                threads[i] = CreateThread(NULL, 65536, sleeper, NULL,
                                         STACK_SIZE_PARAM_IS_A_RESERVATION, &tids[i]);
                if (!threads[i]) exit(9);
            }
            Sleep(200);
            for (int round = 0; round < 5; round++) {
                for (int order = 0; order < 3; order++) {
                    int method = (order + round) % 3;
                    sample(method, count);
                    LARGE_INTEGER start, now;
                    QueryPerformanceCounter(&start);
                    unsigned samples = 0;
                    do {
                        sample(method, count);
                        samples++;
                        QueryPerformanceCounter(&now);
                    } while ((double)(now.QuadPart - start.QuadPart) / frequency.QuadPart < 0.15);
                    double ns = 1e9 * (double)(now.QuadPart - start.QuadPart) / frequency.QuadPart / samples;
                    printf("%zu,%u,%u,%d,%s,%u,%.0f\n", sizeof(void *)*8,count,extra,round,names[method],samples,ns);
                }
            }
            SetEvent(stop_event);
            for (unsigned i = 0; i < total; i++) {
                WaitForSingleObject(threads[i], INFINITE);
                CloseHandle(threads[i]);
            }
            CloseHandle(stop_event);
        }
    }
    free(buffer);
    return 0;
}
