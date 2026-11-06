#include "Tracker.h"
#include <stdlib.h>
#include <string.h>

// Global sparse handle registry matrix (256 x 256)
VOID*** MW_WINS = NULL;

// Initialize the tracker engine memory matrix root
int InitTrackerEngine(void) {
    if (MW_WINS) return 1; // Already initialized

    MW_WINS = (VOID***)malloc(256 * sizeof(VOID**));
    if (!MW_WINS) return 0;

    for (int i = 0; i < 256; i++) {
        MW_WINS[i] = NULL; // Lazy allocation per high tier row
    }
    return 1;
}

// Safely allocate or free window instance memory blocks via handle indices
FG_Handle _FG_Let(FG_Handle hndl, SIZE_T size) {
    // Auto-initialize root engine if it hasn't been booted yet
    if (!MW_WINS) {
        if (!InitTrackerEngine()) return 0;
    }

    UINT32 high = FG_HIGH(hndl);
    UINT32 low  = FG_LOW(hndl);

    if (high >= 256 || low >= 256) return 0;

    // FIX: Lazy allocation restores the high-tier array row if it's missing
    if (!MW_WINS[high]) {
        if (size == 0) return hndl; // Nothing to free if row doesn't exist
        
        MW_WINS[high] = (VOID**)malloc(256 * sizeof(VOID*));
        if (!MW_WINS[high]) return 0;
        
        for (int j = 0; j < 256; j++) {
            MW_WINS[high][j] = NULL;
        }
    }

    // Free existing memory block if present
    if (MW_WINS[high][low]) {
        free(MW_WINS[high][low]);
        MW_WINS[high][low] = NULL;
    }

    // A size allocation of 0 indicates an explicit command to sweep and free memory resources safely
    if (size == 0) {
        return hndl;
    }

    VOID *mem = malloc(size);
    if (!mem) return 0;

    memset(mem, 0, size);
    MW_WINS[high][low] = mem;

    return hndl;
}

// Retrieve a pointer to the tracked window instance
VOID* _FG_Get(FG_Handle handle) {
    if (!MW_WINS) return NULL;
    
    UINT32 h = FG_HIGH(handle);
    UINT32 l = FG_LOW(handle);
    
    if (h >= 256 || l >= 256) return NULL;
    if (!MW_WINS[h]) return NULL;
    
    return MW_WINS[h][l];
}

// Copy data into an existing tracked window instance memory block
VOID _FG_Set(FG_Handle handle, VOID *src, SIZE_T size) {
    VOID *dest = _FG_Get(handle);
    if (dest && src && size > 0) {
        memcpy(dest, src, size);
    }
}

// Clean up tracker matrix tables on engine termination
void ShutdownTrackerEngine(void) {
    if (!MW_WINS) return;

    for (int i = 0; i < 256; i++) {
        if (MW_WINS[i]) {
            for (int j = 0; j < 256; j++) {
                if (MW_WINS[i][j]) {
                    free(MW_WINS[i][j]);
                    MW_WINS[i][j] = NULL;
                }
            }
            free(MW_WINS[i]);
            MW_WINS[i] = NULL;
        }
    }
    free(MW_WINS);
    MW_WINS = NULL;
}
