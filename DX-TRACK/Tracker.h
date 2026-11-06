#ifndef TRACKER_H
#define TRACKER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* =========================================================================
 * TRACKER TYPE DEFINITIONS AND BIT-MASK MACROS
 * ========================================================================= */

// Handle definition mapping for tracking workspace/window instances
typedef UINT64 FG_Handle;

// Extract high and low 32-bit registry indexes out of the 64-bit handle space
#define FG_HIGH(hndl) ((UINT32)(((UINT64)(hndl) >> 32) & 0xFFFFFFFF))
#define FG_LOW(hndl)  ((UINT32)((UINT64)(hndl) & 0xFFFFFFFF))

/* =========================================================================
 * SUBSYSTEM FUNCTION PROTOTYPES
 * ========================================================================= */

// Initialize the sparse handle registry lookup grid
int InitTrackerEngine(void);

// Clean up tracker matrix tables and free memory on engine shutdown
void ShutdownTrackerEngine(void);

// Safely allocate, reallocate, or free context tracking memory blocks
FG_Handle _FG_Let(FG_Handle hndl, SIZE_T size);

// Retrieve a pointer to an active tracked instance block
VOID* _FG_Get(FG_Handle handle);

// Copy raw source data straight into a tracked window instance target
VOID _FG_Set(FG_Handle handle, VOID *src, SIZE_T size);

#endif // TRACKER_H
