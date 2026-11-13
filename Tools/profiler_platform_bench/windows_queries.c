#include <windows.h>
#include <winternl.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define MAX_THREADS 768
#define LENGTH_MISMATCH ((NTSTATUS)0xc0000004L)
static HANDLE stop_event, threads[MAX_THREADS];
static DWORD tids[MAX_THREADS];
static void *buffer;
static ULONG capacity;
static volatile ULONG sink;

typedef struct {
    PDH_HCOUNTER handle;
    PDH_RAW_COUNTER_ITEM_W *items;
    DWORD capacity;
    DWORD count;
} CounterArray;

static PDH_HQUERY pdh_query;
static CounterArray pdh_counters[3];
static unsigned target_slots[2048];

static int pdh_failure(const char *operation, PDH_STATUS status) {
    fprintf(stderr, "PDH SKIP: %s status=0x%08lx\n", operation,
            (unsigned long)status);
    return 0;
}

static void close_pdh(void) {
    if (pdh_query) PdhCloseQuery(pdh_query);
    pdh_query = NULL;
    for (unsigned i = 0; i < 3; i++) {
        free(pdh_counters[i].items);
        pdh_counters[i] = (CounterArray){0};
    }
}

static int open_pdh(unsigned count) {
    const wchar_t *paths[] = {
        L"\\Thread(*)\\Thread State", L"\\Thread(*)\\ID Thread",
        L"\\Thread(*)\\ID Process"
    };
    PDH_STATUS status = PdhOpenQueryW(NULL, 0, &pdh_query);
    if (status != ERROR_SUCCESS) return pdh_failure("open query", status);
    for (unsigned i = 0; i < 3; i++) {
        status = PdhAddEnglishCounterW(pdh_query, paths[i], 0,
                                      &pdh_counters[i].handle);
        if (status != ERROR_SUCCESS) return pdh_failure("add counter", status);
    }
    ZeroMemory(target_slots, sizeof(target_slots));
    for (unsigned i = 0; i < count; i++) {
        unsigned slot = (tids[i] * 2654435761u) & 2047u;
        while (target_slots[slot]) slot = (slot + 1) & 2047u;
        target_slots[slot] = i + 1;
    }
    return 1;
}

static int read_pdh_array(CounterArray *array) {
    for (unsigned retry = 0; retry < 4; retry++) {
        DWORD bytes = array->capacity;
        PDH_STATUS status = PdhGetRawCounterArrayW(array->handle, &bytes,
                                                   &array->count, array->items);
        if (status == ERROR_SUCCESS) return 1;
        if (status != (PDH_STATUS)PDH_MORE_DATA && status != (PDH_STATUS)PDH_INVALID_ARGUMENT) {
            return pdh_failure("read array", status);
        }
        /* A failed nonzero-size call need not return the required size. */
        bytes = 0;
        status = PdhGetRawCounterArrayW(array->handle, &bytes,
                                       &array->count, NULL);
        if (status != (PDH_STATUS)PDH_MORE_DATA || bytes > MAXDWORD - 4096) {
            return pdh_failure("size array", status);
        }
        DWORD capacity = bytes + 4096;
        void *items = realloc(array->items, capacity);
        if (!items) return pdh_failure("allocate array", PDH_MEMORY_ALLOCATION_FAILURE);
        array->items = items;
        array->capacity = capacity;
    }
    return pdh_failure("array changed repeatedly", PDH_MORE_DATA);
}

static PDH_RAW_COUNTER *named_pdh_value(CounterArray *array,
                                       const wchar_t *name, DWORD hint) {
    /* Counter arrays need not have identical order. */
    if (hint < array->count && wcscmp(array->items[hint].szName, name) == 0) {
        return &array->items[hint].RawValue;
    }
    for (DWORD i = 0; i < array->count; i++) {
        if (wcscmp(array->items[i].szName, name) == 0) return &array->items[i].RawValue;
    }
    return NULL;
}

static int valid_pdh_value(const PDH_RAW_COUNTER *value) {
    return value && (value->CStatus == PDH_CSTATUS_VALID_DATA ||
                     value->CStatus == PDH_CSTATUS_NEW_DATA);
}

static int sample_pdh(unsigned count) {
    PDH_STATUS status = PdhCollectQueryData(pdh_query);
    if (status != ERROR_SUCCESS) return pdh_failure("collect", status);
    for (unsigned i = 0; i < 3; i++) {
        if (!read_pdh_array(&pdh_counters[i])) return 0;
    }
    unsigned char seen[MAX_THREADS] = {0};
    unsigned found = 0;
    CounterArray *ids = &pdh_counters[1];
    for (DWORD i = 0; i < ids->count; i++) {
        PDH_RAW_COUNTER_ITEM_W *item = &ids->items[i];
        if (!valid_pdh_value(&item->RawValue) ||
            item->RawValue.FirstValue <= 0 || item->RawValue.FirstValue > MAXDWORD) continue;
        DWORD tid = (DWORD)item->RawValue.FirstValue;
        unsigned slot = (tid * 2654435761u) & 2047u;
        while (target_slots[slot] && tids[target_slots[slot] - 1] != tid) {
            slot = (slot + 1) & 2047u;
        }
        if (!target_slots[slot]) continue;
        unsigned target = target_slots[slot] - 1;
        PDH_RAW_COUNTER *pid = named_pdh_value(&pdh_counters[2], item->szName, i);
        PDH_RAW_COUNTER *state = named_pdh_value(&pdh_counters[0], item->szName, i);
        if (!valid_pdh_value(pid) || !valid_pdh_value(state) || seen[target] ||
            pid->FirstValue != (LONGLONG)GetCurrentProcessId()) {
            return pdh_failure("invalid or duplicate target instance", PDH_INVALID_DATA);
        }
        if (state->FirstValue != 5) {
            fprintf(stderr, "PDH expected waiting state for %lu, got %lld\n",
                    (unsigned long)tid, (long long)state->FirstValue);
            exit(8);
        }
        seen[target] = 1;
        found++;
        sink += (ULONG)state->FirstValue;
    }
    if (found != count) return pdh_failure("target instance missing", PDH_CSTATUS_NO_INSTANCE);
    return 1;
}

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
        (ULONG_PTR)info.ClientId.UniqueThread != (ULONG_PTR)tid ||
        (ULONG_PTR)info.ClientId.UniqueProcess != (ULONG_PTR)GetCurrentProcessId()) {
        fprintf(stderr, "direct status=%lx size=%lu expected=%zu\n", (unsigned long)status,size,sizeof(info));
        exit(7);
    }
    return info.ThreadState;
}

static int sample(int method, unsigned count) {
    if (method == 3) return sample_pdh(count);
    SYSTEM_PROCESS_INFORMATION *all = method == 2 ? snapshot() : NULL;
    for (unsigned i = 0; i < count; i++) {
        ULONG state = method == 1 ? direct_state(tids[i])
            : state_from_snapshot(method == 2 ? all : snapshot(), tids[i]);
        if (state != 5) {fprintf(stderr, "Expected waiting state, got %lu\n",state);exit(8);}
        sink += state;
    }
    return 1;
}

int main(void) {
    const unsigned counts[] = {1, 16, 128};
    const char *names[] = {"baseline_per_thread", "direct_thread", "snapshot_per_sample", "pdh_per_sample"};
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
            int pdh_available = open_pdh(count);
            if (!pdh_available) {
                printf("%zu,%u,%u,-1,pdh_per_sample,0,nan\n", sizeof(void *)*8,count,extra);
            }
            for (int round = 0; round < 5; round++) {
                for (int order = 0; order < 4; order++) {
                    int method = (order + round) % 4;
                    if (method == 3 && !pdh_available) continue;
                    if (!sample(method, count)) {
                        pdh_available = 0;
                        printf("%zu,%u,%u,%d,pdh_per_sample,0,nan\n", sizeof(void *)*8,count,extra,round);
                        continue;
                    }
                    LARGE_INTEGER start, now;
                    QueryPerformanceCounter(&start);
                    unsigned samples = 0;
                    do {
                        if (!sample(method, count)) {
                            pdh_available = 0;
                            break;
                        }
                        samples++;
                        QueryPerformanceCounter(&now);
                    } while ((double)(now.QuadPart - start.QuadPart) / frequency.QuadPart < 0.15);
                    if (method == 3 && !pdh_available) {
                        printf("%zu,%u,%u,%d,pdh_per_sample,0,nan\n", sizeof(void *)*8,count,extra,round);
                        continue;
                    }
                    double ns = 1e9 * (double)(now.QuadPart - start.QuadPart) / frequency.QuadPart / samples;
                    printf("%zu,%u,%u,%d,%s,%u,%.0f\n", sizeof(void *)*8,count,extra,round,names[method],samples,ns);
                }
            }
            close_pdh();
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
