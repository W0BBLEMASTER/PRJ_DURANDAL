#include <stddef.h>

// Only stub the formats that require external dependencies we've disabled
void* flac_open(void) { return NULL; }
void* ogg_open(void) { return NULL; }
void* vorbis_open(void) { return NULL; }
void* mpeg_open(void) { return NULL; }
void* mpeg_init(void) { return NULL; }
// Format files like rx2.c, mpc2k.c, etc. are being compiled, so no stubs needed for them.