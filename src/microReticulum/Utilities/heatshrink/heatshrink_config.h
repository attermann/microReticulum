#ifndef HEATSHRINK_CONFIG_H
#define HEATSHRINK_CONFIG_H

/*
microReticulum Changes:
Avoids malloc, fixes window/lookahead at compile time.
Window=9 (512-byte history) + lookahead=4 trades modest RAM (~3 KB per encoder, ~1 KB per decoder) for a solid compression ratio on text-like MsgPack payloads.
NOTE: Keep these in sync with whatever Utilities/Compress allocates and with the JS decoder's window assumption in webconsole.
*/

/* Should functionality assuming dynamic allocation be used? */
#ifndef HEATSHRINK_DYNAMIC_ALLOC
//#define HEATSHRINK_DYNAMIC_ALLOC 1
#define HEATSHRINK_DYNAMIC_ALLOC 0
#endif

#if HEATSHRINK_DYNAMIC_ALLOC
    /* Optional replacement of malloc/free */
    #define HEATSHRINK_MALLOC(SZ) malloc(SZ)
    #define HEATSHRINK_FREE(P, SZ) free(P)
#else
    /* Required parameters for static configuration */
    //#define HEATSHRINK_STATIC_INPUT_BUFFER_SIZE 32
    #define HEATSHRINK_STATIC_INPUT_BUFFER_SIZE 64
    //#define HEATSHRINK_STATIC_WINDOW_BITS 8
    #define HEATSHRINK_STATIC_WINDOW_BITS 9
    #define HEATSHRINK_STATIC_LOOKAHEAD_BITS 4
#endif

/* Turn on logging for debugging. */
#define HEATSHRINK_DEBUGGING_LOGS 0

/* Use indexing for faster compression. (This requires additional space.) */
#define HEATSHRINK_USE_INDEX 1

#endif
