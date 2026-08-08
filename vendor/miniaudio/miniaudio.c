/* The single translation unit that compiles miniaudio's implementation.
   Everything else includes miniaudio.h as a plain header. */
#define MINIAUDIO_IMPLEMENTATION

/* Playback only: no capture devices, no encoding, and none of the decoders
   the game does not use. Cuts build time and the surface we depend on. */
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER_JOB_THREAD

#include "miniaudio.h"
