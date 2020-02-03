#include "input.h"


void INPUT_Init() {
	PAD_Init();
}

void INPUT_Scan() {
	PAD_ScanPads();
}

u32 INPUT_ButtonsDown() {
	return PAD_ButtonsDown(0);
}

u32 INPUT_ButtonsHeld() {
	return PAD_ButtonsHeld(0);
}
