#ifndef _INPUT_H
#define _INPUT_H

#include <gccore.h>

void INPUT_Init();
void INPUT_Scan();

u32 INPUT_ButtonsDown();
u32 INPUT_ButtonsHeld();

#define	BUTTON_SELECT	PAD_BUTTON_A
#define BUTTON_BACK		PAD_BUTTON_B
#define BUTTON_REBOOT	WPAD_BUTTON_HOME
#define	BUTTON_UP		PAD_BUTTON_UP
#define	BUTTON_DOWN		PAD_BUTTON_DOWN
#define	BUTTON_LEFT		PAD_BUTTON_LEFT
#define	BUTTON_RIGHT	PAD_BUTTON_RIGHT

#endif // _INPUT_H
