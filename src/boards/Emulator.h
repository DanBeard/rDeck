#pragma once
#ifdef RET_PLATFORM_EMU
#include "retHal/RetHal.h"

// declare the HAL struct and the board init code here.
// define in CPP
// Include any macros or other stuff too
// use BOARD_HAL and BOARD_INIT in the main entry point for easy switching.

extern RetHal emuHal;
extern void emuBoardInit();
#define BOARD_HAL emuHal
#define BOARD_INIT emuBoardInit();
#endif
