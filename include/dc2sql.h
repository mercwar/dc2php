#ifndef DC2SQL_H
#define DC2SQL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include "../DX-TRACK/Tracker.h"
#include "../include/dc2jet.h"

#define DC2SQL_API __declspec(dllexport)

// Session structure stored within the 256x256 Tracker matrix
typedef struct {
    SQLHENV henv;
    SQLHDBC hdbc;
    SQLHSTMT hstmt;
    char session_id[64];
    char db_path[MAX_PATH];
    DWORD last_active_tick;
    BOOL is_connected;
} DC2SQL_SESSION;

#ifdef __cplusplus
extern "C" {
#endif

DC2SQL_API SQLRETURN DC2SQL_InitializeEngine(void);
DC2SQL_API FG_Handle DC2SQL_OpenSession(const char* sessionId, const char* dbPath);
DC2SQL_API SQLRETURN DC2SQL_ExecuteQuery(FG_Handle handle, const char* sqlQuery, char* outputBuffer, size_t bufferSize);
DC2SQL_API void DC2SQL_CloseSession(FG_Handle handle);
DC2SQL_API void DC2SQL_ShutdownEngine(void);

#ifdef __cplusplus
}
#endif

#endif // DC2SQL_H