#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include <debug.h>
#include <math.h>

#include <fat.h>
#include <sys/dir.h>
#include <unistd.h>

#include "file_browse.h"
#include "run_dol.h"

#include "input.h"

using namespace std;
static void *xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;

void Stop() {
	while(1) {

		VIDEO_WaitVSync();
		INPUT_Scan();

		int buttonsDown = INPUT_ButtonsDown();
		
		if (buttonsDown & BUTTON_REBOOT) {
			exit(0);
		}

		if (buttonsDown & BUTTON_SELECT) {
			break;
		}
	}
}

int main() {

	std::string filename;
	char filePath[MAXPATHLEN * 2];
	int pathLen;

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode( NULL );
	
	INPUT_Init();

	
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		
	VIDEO_Configure(rmode);
		
	VIDEO_SetNextFramebuffer(xfb);
	
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);
	setvbuf(stdout, NULL , _IONBF, 0);

	iprintf ("\nInitialising FAT...");
	if (fatInitDefault()) {
		printf ("okay\n");
	} else {
		printf ("fail!\n");
		Stop();
	} 

	vector<string> extensionList;
	extensionList.push_back(".dol");
	extensionList.push_back(".argv");

	while(1) {

		filename = browseForFile(extensionList);

		// Construct a command line
		getcwd (filePath, MAXPATHLEN);
		pathLen = strlen (filePath);
		vector<char*> argarray;

		if ( strcasecmp (filename.c_str() + filename.size() - 5, ".argv") == 0) {

			FILE *argfile = fopen(filename.c_str(),"rb");
			char str[MAXPATHLEN], *pstr;
			const char seps[]= "\n\r\t ";

			while( fgets(str, MAXPATHLEN, argfile) ) {
				// Find comment and end string there
				if( (pstr = strchr(str, '#')) )
					*pstr= '\0';
		
				// Tokenize arguments
				pstr= strtok(str, seps);

				while( pstr != NULL ) {
					argarray.push_back(strdup(pstr));
					pstr= strtok(NULL, seps);
				}
			}
			fclose(argfile);
			filename = argarray.at(0);
		} else {
			argarray.push_back(strdup(filename.c_str()));
		} 

		if ( strcasecmp (filename.c_str() + filename.size() - 4, ".dol") != 0 || argarray.size() == 0 ) {
			iprintf("no dol file specified\n");
		} else {
			char *name = argarray.at(0);
			strcpy (filePath + pathLen, name);
			free(argarray.at(0));
			argarray.at(0) = filePath;
			iprintf ("Running %s with %d parameters\n", argarray[0], argarray.size());
			int err = runDOL (argarray[0], argarray.size(), (const char **)&argarray[0]);
			iprintf ("Start failed. Error %i\n", err);

		}

		while(argarray.size() !=0 ) {
			free(argarray.at(0));
			argarray.erase(argarray.begin());
		}

		u32 held;
		do {
			INPUT_Scan();

			held = INPUT_ButtonsDown();
			VIDEO_WaitVSync();
		} while (held & BUTTON_SELECT);


	}
}
