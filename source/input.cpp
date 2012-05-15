#include "input.h"


void INPUT_Init() {
#ifdef __wii__
	WPAD_Init();
#else
	PAD_Init();
#endif
}

void INPUT_Scan() {
#ifdef __wii__
	WPAD_ScanPads();
#else
	PAD_ScanPads();
#endif
}

u32 INPUT_ButtonsDown() {
#ifdef __wii__
	return WPAD_ButtonsDown(0);
#else
	return PAD_ButtonsDown(0);
#endif
}

u32 INPUT_ButtonsHeld() {
#ifdef __wii__
	return WPAD_ButtonsHeld(0);
#else
	return PAD_ButtonsHeld(0);
#endif
}
