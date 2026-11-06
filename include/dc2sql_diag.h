#ifndef DC2SQL_DIAG_H
#define DC2SQL_DIAG_H

#include <windows.h>
#include <stdbool.h>

#ifdef BUILDING_JET_DLL
#define DIAG_API __declspec(dllexport)
#else
#define DIAG_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

DIAG_API void __stdcall diag_set_enabled(bool enabled);
DIAG_API bool __stdcall diag_is_enabled(void);
DIAG_API void __stdcall diag_log_event(const char* category, const char* message);
DIAG_API int  __stdcall diag_get_buffered_count(void);
DIAG_API bool __stdcall diag_flush_reports(char* out_buffer, size_t max_size);
DIAG_API void __stdcall diag_clear_buffer(void);

#ifdef __cplusplus
}
#endif

#endif // DC2SQL_DIAG_H