#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <debug.h>
#include <math.h>
#include <network.h>
#include <fat.h>
#include <sys/dir.h>
#include <unistd.h>
#include <ogcsys.h>
#include <gccore.h>
#include <errno.h>

#include "file_browse.h"
#include "run_dol.h"

#include "input.h"

#define MAXPATHLEN 512

using namespace std;
static void *xfb = NULL;

u32 first_frame = 1;
GXRModeObj *rmode;

void Stop() {
	while(1) {

		VIDEO_WaitVSync();
		INPUT_Scan();

		int buttonsDown = INPUT_ButtonsDown();
		
		if (buttonsDown & PAD_BUTTON_START) {
			exit(0);
		}

		if (buttonsDown & PAD_TRIGGER_Z) {
			break;
		}
	}
}
void bbaConfig() {
	char localip[16] = { 0 };
	char gateway[16] = { 0 };
	char netmask[16] = { 0 };
	s32 ret = if_config(localip, netmask, gateway, TRUE, 20);
	if (ret >= 0) {
		printf("bba setup: ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);
	}
	else {
		iprintf("\nbba setup failed");
	}
}
int downloadFile(std::string apiEndpoint, std::string filename) {
	int sock_descriptor; // integer number to access socket
	struct sockaddr_in serv_addr; // uses predefined sockaddr_in struct
	//struct hostent* server; // from netdb.h to determine host name out of ip address
	char recvBuff[1024];  // Receiving buffer 

	char req[] = "GET /vi/HevHbYbnLHQ/hqdefault.jpg HTTP/1.1"
		"Host: localhost\n"
		"Connection: keep-alive\n"
		"Cache-Control: no-cache\n"
		"Accept: image/jpeg, text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\n"
		"Pragma: no-cache\n"
		"User-Agent: PuppetMaster/1.0 (Nintendo Gamecube; PPC32)\n"
		"Accept-Encoding: gzip,deflate,sdch\n"
		"Accept-Language: en-GB,en;q=0.8,en-US;q=0.6,da;q=0.4\n"
		"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\n\n";

	sock_descriptor = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // SOCK_STREAM = TCP, AF_INET = DOMAIN
	if (sock_descriptor == INVALID_SOCKET) {
		iprintf("\nFailed creating socket");
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);
	serv_addr.sin_addr.s_addr = inet_addr("172.217.17.86");
	/*server = net_
	if (server == nullptr) {
		return 0;
	}*/
	//memcpy((char*)&(serv_addr.sin_addr.s_addr), (char*)(server->h_addr), server->h_length);

	if (net_connect(sock_descriptor, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) < 0) {
		iprintf("\nFailed to connect to server");
	}
	else {
		iprintf ("\nDONE");
	}
	net_write(sock_descriptor, req, sizeof(req));
	FILE* filehandle = fopen(filename.c_str(), "wb");

	int readBytes = 0;
	while (true) {
		memset(recvBuff, 0, 1024);
		if ((readBytes = read(sock_descriptor, recvBuff, sizeof(recvBuff) - 1)) > 0) {
			fwrite(recvBuff, sizeof(recvBuff), 1, filehandle);
		}
		else {
	//		iprintf("\nDownload finished");
			return 0;
		}
	}	
	fclose(filehandle);
	
	iprintf("\nDownload complete");
	return 0;
}

int main() {

	std::string filename;
	char filePath[MAXPATHLEN * 2];
	int pathLen;

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode( NULL );
	
	INPUT_Init();
	
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		
	bbaConfig();

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
			if (held & PAD_BUTTON_X) {
				std::string url = "https://i.ytimg.com/vi/HevHbYbnLHQ/hqdefault.jpg";
				downloadFile(url);
			}
			VIDEO_WaitVSync();
		} while (held & PAD_BUTTON_START);
	}
}
