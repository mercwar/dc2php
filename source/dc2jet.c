#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Tracker.h"
#include "dc2jet.h"
#include "dc2sql_diag.h"

struct JetContext {
    SQLHENV henv;
    SQLHDBC hdbc;
    SQLHSTMT hstmt;
    char db_path[MAX_PATH];
    char passkey[128];
    DWORD last_active_tick;
    BOOL is_connected;
};

static void EnsureTrackerInitialized(void) {
    if (!MW_WINS) {
        MW_WINS = (VOID***)malloc(256 * sizeof(VOID**));
        if (!MW_WINS) return;
        memset(MW_WINS, 0, 256 * sizeof(VOID**));
        for (int i = 0; i < 256; i++) {
            MW_WINS[i] = (VOID**)malloc(256 * sizeof(VOID*));
            if (MW_WINS[i]) memset(MW_WINS[i], 0, 256 * sizeof(VOID*));
        }
    }
}

// Native DAO 3.60 COM-based exclusive Jet database generator with explicit EngineType/dbVersion40 (Jet 4.0)
static BOOL EnsureDatabaseExistsDAO(const char* db_path, const char* passkey) {
    DWORD attr = GetFileAttributesA(db_path);
    if (attr != INVALID_FILE_ATTRIBUTES && attr != FILE_ATTRIBUTE_DIRECTORY) {
        diag_log_event("DAO", "Exclusive Jet database container already active on filesystem.");
        return TRUE;
    }

    diag_log_event("DAO", "Initializing COM Apartment for exclusive Jet 4.0 provisioning...");
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BOOL created = FALSE;

    CLSID clsid;
    hr = CLSIDFromProgID(L"DAO.DBEngine.36", &clsid);
    if (FAILED(hr)) {
        diag_log_event("DAO", "DAO.DBEngine.36 ProgID unavailable; attempting 12.0 fallback...");
        hr = CLSIDFromProgID(L"DAO.DBEngine.120", &clsid);
    }

    if (SUCCEEDED(hr)) {
        IDispatch* pDBEngine = NULL;
        hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void**)&pDBEngine);
        if (SUCCEEDED(hr) && pDBEngine) {
            
            IDispatch* pWorkspace = NULL;
            VARIANTARG vWsArg[2];
            VARIANT vWsRes;
            DISPPARAMS dpWs = { vWsArg, NULL, 2, 0 };

            VariantInit(&vWsArg[1]);
            vWsArg[1].vt = VT_BSTR;
            vWsArg[1].bstrVal = SysAllocString(L"#DefaultUser#");

            VariantInit(&vWsArg[0]);
            vWsArg[0].vt = VT_BSTR;
            vWsArg[0].bstrVal = SysAllocString(L"Admin");

            VariantInit(&vWsRes);
            DISPID dispidCreateWs;
            OLECHAR* szCreateWs = L"CreateWorkspace";
            
            if (SUCCEEDED(pDBEngine->lpVtbl->GetIDsOfNames(pDBEngine, &IID_NULL, &szCreateWs, 1, LOCALE_USER_DEFAULT, &dispidCreateWs))) {
                if (SUCCEEDED(pDBEngine->lpVtbl->Invoke(pDBEngine, dispidCreateWs, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dpWs, &vWsRes, NULL, NULL))) {
                    if (vWsRes.vt == VT_DISPATCH) {
                        pWorkspace = vWsRes.pdispVal;
                    }
                }
            }
            SysFreeString(vWsArg[1].bstrVal);
            SysFreeString(vWsArg[0].bstrVal);

            IDispatch* pTargetWorkspace = pWorkspace ? pWorkspace : pDBEngine;

            VARIANTARG vArg[3];
            VARIANT vRes;
            DISPPARAMS dp = { vArg, NULL, 3, 0 };
            
            WCHAR wPath[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, db_path, -1, wPath, MAX_PATH);

            WCHAR wLocale[256];
            if (passkey && passkey[0] != '\0') {
                WCHAR wPass[128];
                MultiByteToWideChar(CP_ACP, 0, passkey, -1, wPass, 128);
                swprintf_s(wLocale, 256, L";LANGID=0x0409;CP=1252;COUNTRY=0;PWD=%s", wPass);
            } else {
                wcscpy_s(wLocale, 256, L";LANGID=0x0409;CP=1252;COUNTRY=0");
            }

            VariantInit(&vArg[2]);
            vArg[2].vt = VT_BSTR;
            vArg[2].bstrVal = SysAllocString(wPath);

            VariantInit(&vArg[1]);
            vArg[1].vt = VT_BSTR;
            vArg[1].bstrVal = SysAllocString(wLocale);

            VariantInit(&vArg[0]);
            vArg[0].vt = VT_I4;
            vArg[0].lVal = 64; // dbVersion40 flag (forces Jet 4.0 format exclusively)

            VariantInit(&vRes);
            DISPID dispid;
            OLECHAR* szMember = L"CreateDatabase";
            hr = pTargetWorkspace->lpVtbl->GetIDsOfNames(pTargetWorkspace, &IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
            
            if (SUCCEEDED(hr)) {
                hr = pTargetWorkspace->lpVtbl->Invoke(pTargetWorkspace, dispid, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dp, &vRes, NULL, NULL);
                if (SUCCEEDED(hr)) {
                    if (vRes.vt == VT_DISPATCH && vRes.pdispVal) {
                        DISPID dispidClose;
                        OLECHAR* szClose = L"Close";
                        if (SUCCEEDED(vRes.pdispVal->lpVtbl->GetIDsOfNames(vRes.pdispVal, &IID_NULL, &szClose, 1, LOCALE_USER_DEFAULT, &dispidClose))) {
                            DISPPARAMS dpClose = { NULL, NULL, 0, 0 };
                            vRes.pdispVal->lpVtbl->Invoke(vRes.pdispVal, dispidClose, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dpClose, NULL, NULL, NULL);
                        }
                        vRes.pdispVal->lpVtbl->Release(vRes.pdispVal);
                        diag_log_event("DAO", "Exclusive Jet 4.0 container created and closed cleanly.");
                    }
                    created = TRUE;
                }
            }

            SysFreeString(vArg[2].bstrVal);
            SysFreeString(vArg[1].bstrVal);

            if (pWorkspace) {
                pWorkspace->lpVtbl->Release(pWorkspace);
            }
            pDBEngine->lpVtbl->Release(pDBEngine);
        }
    }

    if (SUCCEEDED(hr) || hr == S_FALSE) {
        CoUninitialize();
    }

    Sleep(50);
    return created;
}

static void BootstrapJetDatabaseSchema(SQLHDBC hdbc) {
    SQLHSTMT hstmt = SQL_NULL_HSTMT;
    if (SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt) != SQL_SUCCESS) return;

    const char* createTableSQL = 
        "CREATE TABLE SystemNodes ("
        "   NodeID COUNTER PRIMARY KEY, "
        "   NodeVersion TEXT(64), "
        "   Operator TEXT(64), "
        "   Details MEMO"
        ")";
    
    // Will fail gracefully if table already exists, which is expected on subsequent connections
    SQLExecDirectA(hstmt, (SQLCHAR*)createTableSQL, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    // FIX: Check if records already exist before seeding to prevent row accumulation
    bool hasRows = false;
    if (SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt) == SQL_SUCCESS) {
        if (SQLExecDirectA(hstmt, (SQLCHAR*)"SELECT COUNT(*) FROM SystemNodes", SQL_NTS) == SQL_SUCCESS) {
            if (SQLFetch(hstmt) == SQL_SUCCESS) {
                SQLLEN countVal = 0;
                SQLLEN indicator = 0;
                SQLGetData(hstmt, 1, SQL_C_LONG, &countVal, sizeof(countVal), &indicator);
                if (countVal > 0) {
                    hasRows = true;
                }
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    }

    // Only seed if table is completely empty
    if (!hasRows) {
        if (SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt) == SQL_SUCCESS) {
            const char* insertRecordSQL = 
                "INSERT INTO SystemNodes (NodeVersion, Operator, Details) "
                "VALUES ('mercwar', 'joseph michael catalano', 'mercwar ai')";
            SQLExecDirectA(hstmt, (SQLCHAR*)insertRecordSQL, SQL_NTS);
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            diag_log_event("SCHEMA", "SystemNodes table bootstrapped and seeded successfully.");
        }
    } else {
        diag_log_event("SCHEMA", "SystemNodes table already contains records; skipping duplicate seed.");
    }
}
JET_API JetContext* __stdcall jet_initialize(const char* base_dir, const char* db_filename, const char* passkey) {
    EnsureTrackerInitialized();

    JetContext* ctx = (JetContext*)malloc(sizeof(JetContext));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(JetContext));

    if (base_dir && base_dir[0] != '\0') {
        snprintf(ctx->db_path, sizeof(ctx->db_path), "%s\\%s", base_dir, db_filename);
    } else {
        snprintf(ctx->db_path, sizeof(ctx->db_path), "%s", db_filename);
    }

    if (passkey && passkey[0] != '\0') {
        strncpy_s(ctx->passkey, sizeof(ctx->passkey), passkey, _TRUNCATE);
    }

    diag_log_event("JET", "Ensuring exclusive Jet database container exists...");
    EnsureDatabaseExistsDAO(ctx->db_path, ctx->passkey);

    ctx->last_active_tick = GetTickCount();

    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HENV, &ctx->henv) != SQL_SUCCESS) {
        diag_log_event("JET", "CRITICAL: Failed to allocate ODBC Environment handle.");
        free(ctx);
        return NULL;
    }
    SQLSetEnvAttr(ctx->henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    if (SQLAllocHandle(SQL_HANDLE_DBC, ctx->henv, &ctx->hdbc) != SQL_SUCCESS) {
        diag_log_event("JET", "CRITICAL: Failed to allocate ODBC Connection handle.");
        SQLFreeHandle(SQL_HANDLE_ENV, ctx->henv);
        free(ctx);
        return NULL;
    }

    char connStr[512];
    if (ctx->passkey[0] != '\0') {
        snprintf(connStr, sizeof(connStr), 
                 "Driver={Microsoft Access Driver (*.mdb)};DBQ=%s;Uid=Admin;Pwd=%s;", 
                 ctx->db_path, ctx->passkey);
    } else {
        snprintf(connStr, sizeof(connStr), 
                 "Driver={Microsoft Access Driver (*.mdb)};DBQ=%s;Uid=Admin;Pwd=;", 
                 ctx->db_path);
    }

    SQLCHAR outConn[1024];
    SQLSMALLINT outLen = 0;
    SQLRETURN ret = SQLDriverConnectA(ctx->hdbc, NULL, (SQLCHAR*)connStr, SQL_NTS, 
                                     outConn, sizeof(outConn), &outLen, SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        diag_log_event("JET", "CRITICAL: SQLDriverConnectA failed to establish exclusive ODBC session.");
        SQLFreeHandle(SQL_HANDLE_DBC, ctx->hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, ctx->henv);
        free(ctx);
        return NULL;
    }

    ctx->is_connected = TRUE;
    diag_log_event("JET", "Exclusive ODBC connection established successfully.");

    BootstrapJetDatabaseSchema(ctx->hdbc);

    for (int h = 0; h < 256; h++) {
        for (int l = 0; l < 256; l++) {
            if (MW_WINS && MW_WINS[h] && MW_WINS[h][l] == NULL) {
                MW_WINS[h][l] = ctx;
                return ctx;
            }
        }
    }

    return ctx;
}

JET_API bool __stdcall jet_route_path(const char* prefix, const char* input_name, const char* suffix, char* out_buffer, size_t max_size) {
    if (!out_buffer || max_size == 0) return false;
    int written = snprintf(out_buffer, max_size, "%s\\%s%s", 
                           prefix ? prefix : "", 
                           input_name ? input_name : "", 
                           suffix ? suffix : "");
    return (written > 0 && (size_t)written < max_size);
}

JET_API bool __stdcall jet_execute_action(JetContext* ctx, const char* sql_command) {
    if (!ctx || !ctx->is_connected || !sql_command) return false;
    ctx->last_active_tick = GetTickCount();

    SQLHSTMT hstmt = SQL_NULL_HSTMT;
    if (SQLAllocHandle(SQL_HANDLE_STMT, ctx->hdbc, &hstmt) != SQL_SUCCESS) {
        return false;
    }

    SQLRETURN ret = SQLExecDirectA(hstmt, (SQLCHAR*)sql_command, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    return (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
}

JET_API bool __stdcall jet_execute_select(JetContext* ctx, const char* sql_command, JetRowCallback callback, void* user_data) {
    if (!ctx || !ctx->is_connected || !sql_command || !callback) return false;
    ctx->last_active_tick = GetTickCount();

    SQLHSTMT hstmt = SQL_NULL_HSTMT;
    if (SQLAllocHandle(SQL_HANDLE_STMT, ctx->hdbc, &hstmt) != SQL_SUCCESS) {
        return false;
    }

    SQLRETURN ret = SQLExecDirectA(hstmt, (SQLCHAR*)sql_command, SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return false;
    }

    SQLSMALLINT num_cols = 0;
    SQLNumResultCols(hstmt, &num_cols);

    if (num_cols <= 0) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return true;
    }

    const char** col_names = (const char**)malloc(num_cols * sizeof(char*));
    char** col_vals_buf = (char**)malloc(num_cols * sizeof(char*));
    
    if (!col_names || !col_vals_buf) {
        if (col_names) free(col_names);
        if (col_vals_buf) free(col_vals_buf);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return false;
    }

    for (SQLSMALLINT i = 0; i < num_cols; i++) {
        char nameBuf[256] = {0};
        SQLSMALLINT nameLen = 0;
        SQLDescribeColA(hstmt, i + 1, (SQLCHAR*)nameBuf, sizeof(nameBuf), &nameLen, NULL, NULL, NULL, NULL);
        col_names[i] = _strdup(nameBuf[0] != '\0' ? nameBuf : "Col");
        col_vals_buf[i] = (char*)malloc(2048);
        if (col_vals_buf[i]) {
            memset(col_vals_buf[i], 0, 2048);
        }
    }

    bool success = true;
    while (SQLFetch(hstmt) == SQL_SUCCESS) {
        for (SQLSMALLINT i = 0; i < num_cols; i++) {
            SQLLEN indicator = 0;
            if (col_vals_buf[i]) {
                memset(col_vals_buf[i], 0, 2048);
                SQLGetData(hstmt, i + 1, SQL_C_CHAR, col_vals_buf[i], 2048, &indicator);
                if (indicator == SQL_NULL_DATA) {
                    col_vals_buf[i][0] = '\0';
                }
            }
        }

        if (!callback(num_cols, col_names, (const char**)col_vals_buf, user_data)) {
            success = false;
            break;
        }
    }

    for (SQLSMALLINT i = 0; i < num_cols; i++) {
        if (col_names[i]) free((void*)col_names[i]);
        if (col_vals_buf[i]) free(col_vals_buf[i]);
    }
    free(col_names);
    free(col_vals_buf);

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    return success;
}

JET_API void __stdcall jet_shutdown(JetContext* ctx) {
    if (!ctx) return;
    diag_log_event("JET", "Shutting down Jet context and disconnecting ODBC...");
    if (ctx->is_connected) {
        SQLDisconnect(ctx->hdbc);
    }
    if (ctx->hdbc) SQLFreeHandle(SQL_HANDLE_DBC, ctx->hdbc);
    if (ctx->henv) SQLFreeHandle(SQL_HANDLE_ENV, ctx->henv);

    if (MW_WINS) {
        for (int i = 0; i < 256; i++) {
            if (MW_WINS[i]) {
                for (int j = 0; j < 256; j++) {
                    if (MW_WINS[i][j] == ctx) {
                        MW_WINS[i][j] = NULL;
                    }
                }
            }
        }
    }
    free(ctx);
    diag_log_event("JET", "Jet context successfully destroyed.");
}

JET_API void CALLBACK RunJetTest(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
    MessageBoxA(hwnd, "Exclusive DAO 3.60 Jet 4.0 Workspace Active & Ready.", "DC2JET Engine", MB_OK | MB_ICONINFORMATION);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            EnsureTrackerInitialized();
            break;
        case DLL_PROCESS_DETACH:
            ShutdownTrackerEngine();
            break;
    }
    return TRUE;
}