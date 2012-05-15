#ifndef _INPUT_H
#define _INPUT_H

#include <gccore.h>

#ifdef __wii__
#include <wiiuse/wpad.h>
#endif


void INPUT_Init();
void INPUT_Scan();

u32 INPUT_ButtonsDown();
u32 INPUT_ButtonsHeld();

#ifdef __wii__
#define	BUTTON_SELECT	WPAD_BUTTON_2
#define BUTTON_BACK		WPAD_BUTTON_1
#define BUTTON_REBOOT	WPAD_BUTTON_HOME
#define	BUTTON_UP		WPAD_BUTTON_RIGHT
#define	BUTTON_DOWN		WPAD_BUTTON_LEFT
#define	BUTTON_LEFT		WPAD_BUTTON_UP
#define	BUTTON_RIGHT	WPAD_BUTTON_DOWN

#else

#define	BUTTON_SELECT	PAD_BUTTON_A
#define BUTTON_BACK		PAD_BUTTON_B
#define BUTTON_REBOOT	WPAD_BUTTON_HOME
#define	BUTTON_UP		PAD_BUTTON_UP
#define	BUTTON_DOWN		PAD_BUTTON_DOWN
#define	BUTTON_LEFT		PAD_BUTTON_LEFT
#define	BUTTON_RIGHT	PAD_BUTTON_RIGHT

#endif

#endif // _INPUT_H
