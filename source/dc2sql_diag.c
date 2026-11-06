#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "dc2sql_diag.h"

#define MAX_DIAG_ENTRIES 256
#define MAX_MSG_LEN 512

typedef struct {
    DWORD timestamp;
    char category[64];
    char message[MAX_MSG_LEN];
} DiagEntry;

static volatile LONG g_diag_enabled = 0;
static DiagEntry g_diag_buffer[MAX_DIAG_ENTRIES];
static volatile LONG g_diag_head = 0;
static volatile LONG g_diag_tail = 0;
static CRITICAL_SECTION g_diag_lock;
static BOOL g_lock_initialized = FALSE;

static void EnsureDiagLock(void) {
    if (!g_lock_initialized) {
        // Use a static initialization block or initialize safely on first use
        InitializeCriticalSectionAndSpinCount(&g_diag_lock, 4000);
        g_lock_initialized = TRUE;
    }
}

DIAG_API void __stdcall diag_set_enabled(bool enabled) {
    InterlockedExchange(&g_diag_enabled, enabled ? 1 : 0);
}

DIAG_API bool __stdcall diag_is_enabled(void) {
    return (g_diag_enabled != 0);
}

DIAG_API void __stdcall diag_log_event(const char* category, const char* message) {
    if (!diag_is_enabled() || !message) return;

    EnsureDiagLock();
    EnterCriticalSection(&g_diag_lock);

    LONG head = g_diag_head;
    LONG next_head = (head + 1) % MAX_DIAG_ENTRIES;

    // If buffer is full, advance tail (drop oldest log)
    if (next_head == g_diag_tail) {
        g_diag_tail = (g_diag_tail + 1) % MAX_DIAG_ENTRIES;
    }

    g_diag_buffer[head].timestamp = GetTickCount();
    strncpy_s(g_diag_buffer[head].category, sizeof(g_diag_buffer[head].category), category ? category : "SYSTEM", _TRUNCATE);
    strncpy_s(g_diag_buffer[head].message, sizeof(g_diag_buffer[head].message), message, _TRUNCATE);

    g_diag_head = next_head;

    LeaveCriticalSection(&g_diag_lock);
}

DIAG_API int __stdcall diag_get_buffered_count(void) {
    EnsureDiagLock();
    EnterCriticalSection(&g_diag_lock);
    
    LONG head = g_diag_head;
    LONG tail = g_diag_tail;
    int count = (head >= tail) ? (head - tail) : (MAX_DIAG_ENTRIES - tail + head);

    LeaveCriticalSection(&g_diag_lock);
    return count;
}

DIAG_API bool __stdcall diag_flush_reports(char* out_buffer, size_t max_size) {
    if (!out_buffer || max_size == 0) return false;

    EnsureDiagLock();
    EnterCriticalSection(&g_diag_lock);

    size_t offset = 0;
    offset += (size_t)snprintf(out_buffer + offset, max_size - offset, "=== DC2SQL DIAGNOSTIC REPORT BUFFER ===\r\n");

    LONG tail = g_diag_tail;
    LONG head = g_diag_head;

    while (tail != head && offset < max_size - 128) {
        DiagEntry* entry = &g_diag_buffer[tail];
        int written = snprintf(out_buffer + offset, max_size - offset, 
                               "[%lu ms] [%s] %s\r\n", 
                               entry->timestamp, entry->category, entry->message);
        if (written > 0) {
            offset += (size_t)written;
        }
        tail = (tail + 1) % MAX_DIAG_ENTRIES;
    }

    // Reset ring buffer indices after flushing
    g_diag_tail = head;

    LeaveCriticalSection(&g_diag_lock);
    return true;
}

DIAG_API void __stdcall diag_clear_buffer(void) {
    EnsureDiagLock();
    EnterCriticalSection(&g_diag_lock);
    g_diag_head = 0;
    g_diag_tail = 0;
    LeaveCriticalSection(&g_diag_lock);
}