#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct JetContext JetContext;
typedef bool (__stdcall *JetRowCallback)(int col_count, const char** col_names, const char** col_values, void* user_data);

typedef JetContext* (__stdcall *pfn_jet_initialize)(const char*, const char*, const char*);
typedef bool (__stdcall *pfn_jet_execute_action)(JetContext*, const char*);
typedef bool (__stdcall *pfn_jet_execute_select)(JetContext*, const char*, JetRowCallback, void*);
typedef void (__stdcall *pfn_jet_shutdown)(JetContext*);

// Diagnostics function pointer typedefs
typedef void (__stdcall *pfn_diag_set_enabled)(bool);
typedef bool (__stdcall *pfn_diag_is_enabled)(void);
typedef int  (__stdcall *pfn_diag_get_buffered_count)(void);
typedef bool (__stdcall *pfn_diag_flush_reports)(char*, size_t);
typedef void (__stdcall *pfn_diag_clear_buffer)(void);

bool __stdcall PrintRowCallback(int col_count, const char** col_names, const char** col_values, void* user_data) {
    int* rowNum = (int*)user_data;
    printf("[Row %d]\n", (*rowNum)++);
    for (int i = 0; i < col_count; i++) {
        printf("  %s = %s\n", col_names[i], col_values[i] ? col_values[i] : "NULL");
    }
    return true;
}

void PrintUsage(void) {
    printf("DC2JET Command-Line SQL Runner v1.2 (DAO 3.60 & Diagnostics Powered)\n");
    printf("Syntax: dc2jet_cli.exe -db <database.mdb> -query \"<SQL>\" [-passkey <pwd>] [-diag]\n");
}

int main(int argc, char* argv[]) {
    char dbPath[MAX_PATH] = {0};
    char sqlQuery[2048] = {0};
    char passkey[128] = {0};
    bool enableDiag = false;

    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-db") == 0 && i + 1 < argc) {
            strncpy_s(dbPath, sizeof(dbPath), argv[i + 1], _TRUNCATE);
            i++;
        } else if (strcmp(argv[i], "-query") == 0 && i + 1 < argc) {
            strncpy_s(sqlQuery, sizeof(sqlQuery), argv[i + 1], _TRUNCATE);
            i++;
        } else if (strcmp(argv[i], "-passkey") == 0 && i + 1 < argc) {
            strncpy_s(passkey, sizeof(passkey), argv[i + 1], _TRUNCATE);
            i++;
        } else if (strcmp(argv[i], "-diag") == 0) {
            enableDiag = true;
        }
    }

    if (dbPath[0] == '\0' || sqlQuery[0] == '\0') {
        printf("Error: Missing required parameters [-db] or [-query].\n");
        PrintUsage();
        return 1;
    }

    HMODULE hDll = LoadLibraryA("dc2sql.dll");
    if (!hDll) {
        printf("Error: Failed to load dc2sql.dll (Error code: %lu)\n", GetLastError());
        return 1;
    }

    // Resolve core engine procedures
    pfn_jet_initialize jet_init = (pfn_jet_initialize)GetProcAddress(hDll, "jet_initialize");
    pfn_jet_execute_action jet_exec_act = (pfn_jet_execute_action)GetProcAddress(hDll, "jet_execute_action");
    pfn_jet_execute_select jet_exec_sel = (pfn_jet_execute_select)GetProcAddress(hDll, "jet_execute_select");
    pfn_jet_shutdown jet_shut = (pfn_jet_shutdown)GetProcAddress(hDll, "jet_shutdown");

    // Resolve diagnostics procedures
    pfn_diag_set_enabled diag_set = (pfn_diag_set_enabled)GetProcAddress(hDll, "diag_set_enabled");
    pfn_diag_flush_reports diag_flush = (pfn_diag_flush_reports)GetProcAddress(hDll, "diag_flush_reports");

    if (!jet_init || (!jet_exec_act && !jet_exec_sel) || !jet_shut) {
        printf("Error: Failed to resolve required DLL export procedures.\n");
        FreeLibrary(hDll);
        return 1;
    }

    // Enable telemetry tracking if requested via -diag flag
    if (diag_set && enableDiag) {
        diag_set(true);
        printf("[Diagnostics] Telemetry logging enabled.\n");
    }

    JetContext* ctx = jet_init("", dbPath, passkey[0] != '\0' ? passkey : NULL);
    if (!ctx) {
        printf("Error: Failed to open workspace session for database: %s\n", dbPath);
        
        // Dump telemetry buffer on failure if diagnostics are active
        if (diag_flush) {
            char diagBuffer[8192] = {0};
            if (diag_flush(diagBuffer, sizeof(diagBuffer))) {
                printf("\n%s\n", diagBuffer);
            }
        }

        FreeLibrary(hDll);
        return 1;
    }

    printf("--- Executing DAO Jet SQL: %s ---\n", sqlQuery);
    int rowCounter = 1;

    if (_strnicmp(sqlQuery, "SELECT", 6) == 0) {
        bool success = jet_exec_sel(ctx, sqlQuery, PrintRowCallback, &rowCounter);
        if (!success) {
            printf("Error executing SELECT statement.\n");
        }
    } else {
        bool success = jet_exec_act(ctx, sqlQuery);
        if (success) {
            printf("Action query executed successfully.\n");
        } else {
            printf("Error executing action query.\n");
        }
    }

    jet_shut(ctx);

    // Flush and display diagnostics telemetry log report if enabled
    if (diag_flush && enableDiag) {
        char diagBuffer[8192] = {0};
        if (diag_flush(diagBuffer, sizeof(diagBuffer))) {
            printf("\n%s\n", diagBuffer);
        }
    }

    FreeLibrary(hDll);
    return 0;
}