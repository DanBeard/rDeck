/**
 * rdeck_wrapper.cpp - Wrapper to include rdeck.ino for native builds
 *
 * PlatformIO doesn't process .ino files for native platform builds,
 * so we include the content here as regular C++.
 */

#ifdef RET_PLATFORM_EMU
#include "rdeck.ino"
#endif
