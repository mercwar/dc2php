#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Tracker.h"
#include "dc2jet.h"
#include "dc2sql_diag.h"

#define DC2SQL_API __declspec(dllexport)

typedef struct {
    char session_id[64];
    JetContext* jet_ctx;
    DWORD created_tick;
    BOOL active;
} DC2SQL_SessionNode;

DC2SQL_API FG_Handle __stdcall dc2sql_open_session(const char* session_key, const char* db_path, const char* passkey) {
    if (!session_key || !db_path) {
        diag_log_event("SESSION", "Failed to open session: missing session_key or db_path.");
        return 0;
    }

    diag_log_event("SESSION", "Initializing Jet engine context for new session...");
    JetContext* j_ctx = jet_initialize(NULL, db_path, passkey);
    if (!j_ctx) {
        diag_log_event("SESSION", "CRITICAL: jet_initialize failed to return valid context.");
        return 0;
    }

    DC2SQL_SessionNode* node = (DC2SQL_SessionNode*)malloc(sizeof(DC2SQL_SessionNode));
    if (!node) {
        diag_log_event("SESSION", "CRITICAL: Memory allocation failed for session node.");
        jet_shutdown(j_ctx);
        return 0;
    }

    strncpy_s(node->session_id, sizeof(node->session_id), session_key, _TRUNCATE);
    node->jet_ctx = j_ctx;
    node->created_tick = GetTickCount();
    node->active = TRUE;

    if (MW_WINS) {
        for (int h = 0; h < 256; h++) {
            for (int l = 0; l < 256; l++) {
                if (MW_WINS[h] && MW_WINS[h][l] == NULL) {
                    MW_WINS[h][l] = node;
                    diag_log_event("SESSION", "Session node successfully bound to tracker matrix index.");
                    return (FG_Handle)(((UINT_PTR)h << 16) | (UINT_PTR)l);
                }
            }
        }
    }

    diag_log_event("SESSION", "CRITICAL: Tracker matrix table is fully saturated.");
    free(node);
    jet_shutdown(j_ctx);
    return 0;
}

DC2SQL_API bool __stdcall dc2sql_validate_session(FG_Handle handle) {
    DC2SQL_SessionNode* node = (DC2SQL_SessionNode*)_FG_Get(handle);
    bool valid = (node && node->active && node->jet_ctx != NULL);
    if (!valid) {
        diag_log_event("VALIDATION", "Session handle validation failed.");
    }
    return valid;
}

DC2SQL_API bool __stdcall dc2sql_execute_router(FG_Handle handle, const char* sql_command, JetRowCallback callback, void* user_data) {
    if (!dc2sql_validate_session(handle)) return false;
    DC2SQL_SessionNode* node = (DC2SQL_SessionNode*)_FG_Get(handle);

    if (_strnicmp(sql_command, "SELECT", 6) == 0) {
        diag_log_event("ROUTER", "Executing SELECT query via select pipeline.");
        return jet_execute_select(node->jet_ctx, sql_command, callback, user_data);
    } else {
        diag_log_event("ROUTER", "Executing Action statement via action pipeline.");
        return jet_execute_action(node->jet_ctx, sql_command);
    }
}

DC2SQL_API void __stdcall dc2sql_close_session(FG_Handle handle) {
    DC2SQL_SessionNode* node = (DC2SQL_SessionNode*)_FG_Get(handle);
    if (node) {
        diag_log_event("SESSION", "Closing active session and releasing Jet context...");
        if (node->jet_ctx) {
            jet_shutdown(node->jet_ctx);
            node->jet_ctx = NULL;
        }
        node->active = FALSE;
        free(node);
    }
    _FG_Let(handle, 0);
    diag_log_event("SESSION", "Session successfully torn down and unregistered.");
}