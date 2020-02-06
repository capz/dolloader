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
#include <array>

#include "file_browse.h"
#include "run_dol.h"
#include "input.h"
#include "dns.h"

#define MAXPATHLEN 512

static void* xfb = NULL;

u32 first_frame = 1;
GXRModeObj* rmode;

FILE* logFileHandle = nullptr;
#define LOGBUFFERSIZE 5
std::string logbuffer[LOGBUFFERSIZE];
int logEntries = 0;

std::string getWorkingPath() {
	char cwdbuff[1024];
	return std::string(getcwd(cwdbuff, sizeof(char) * 1024));
}
void setupLog(std::string logfilename) {
	if (logFileHandle != nullptr) return;
	logFileHandle = fopen((getWorkingPath() + "/" + logfilename).c_str(), "a+");
}
void exitLog() {
	if (logFileHandle == nullptr) return;
	fclose(logFileHandle);
	logFileHandle = nullptr;
}
void simple_log(std::string message) {
	//time_t now = time(0);
	// convert now to string form
	/*std::string timestring(ctime(&now));*/
	std::string m(message + "\n");
	fwrite(m.c_str(), sizeof(char), sizeof(char) * m.length(), logFileHandle);
}
void log_flush() {
	if ((logEntries % LOGBUFFERSIZE) == (LOGBUFFERSIZE - 1)) {
		setupLog("log.txt");
		for (auto& entry : logbuffer) {
			if (entry.length() < 1) continue;
			std::string m(entry + "\n");

			fwrite(m.c_str(), sizeof(char), sizeof(char) * m.length(), logFileHandle);
			entry = "";
		}
		exitLog();
		logEntries = 0;
	}
}
void log(std::string message, bool directWriteMode = false) {
	if ((logEntries % LOGBUFFERSIZE) == (LOGBUFFERSIZE - 1) || directWriteMode) {
		log_flush();
	}

	time_t now = time(0);

	// convert now to string form
	std::string timestring(ctime(&now));
	logbuffer[logEntries++ % LOGBUFFERSIZE] = std::move(timestring.substr(0, timestring.length() - 1) + " | " + message);
}

void Stop() {
	while (1) {

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
void touchFile(std::string fullPathFilename) {
	FILE* filehandle = fopen(fullPathFilename.c_str(), "b+");
	fwrite("123", sizeof(char), sizeof(char) * 3, filehandle);
	fclose(filehandle);
}
std::string getHostname(std::string url) {
	const auto domainStart = url.find_first_of('/');
	const auto domainEnd = url.substr(domainStart).find_first_of('/');
	return url.substr(domainStart, domainEnd - domainStart);
}
void bbaConfig() {
	char localip[16] = { 0 };
	char gateway[16] = { 0 };
	char netmask[16] = { 0 };
	s32 ret = if_config(localip, netmask, gateway, TRUE, 20);
	if (ret >= 0) {
		//printf("bba setup: ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);
		log("bba setup success!");
	}
	else {
		log("bba setup failed");
	}
	log("setting dns servers from config..", true);
	pm_getDnsServers((getWorkingPath() + "/resolv.conf").c_str());
	for (auto dnsEntry : dns_servers) {
		log(std::string("DNS: ") + dnsEntry);
	}
	log("done", true);
}
int downloadFile(std::string apiEndpoint, std::string filename) {
	int sock_descriptor; // integer number to access socket
	log("endpoint dns lookup...", true);
	struct sockaddr_in serv_addr = pm_gethostbyname((unsigned char*)getHostname(apiEndpoint).c_str(), T_IPv4ADDRESS);
	log("got address", true);
	//struct hostent* server; // from netdb.h to determine host name out of ip address
	char recvBuff[1024];  // Receiving buffer
	std::string header = "GET " + apiEndpoint + " HTTP/1.1"
		"User-Agent: PuppetMaster/1.0 (Nintendo Gamecube; PPC32)\n"
		"Accept-Encoding: gzip, deflate, br\n"
		"Accept-Language: en-GB,en;q=0.8,en-US;q=0.6,da;q=0.4\n"
		"Connection: keep-alive\n"
		"Upgrade-Insecure-Requests: 1\n"
		"Pragma: no-cache\n"
		"Cache - Control : no - cache\n"
		"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\n\n";

	sock_descriptor = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // SOCK_STREAM = TCP, AF_INET = DOMAIN
	if (sock_descriptor == INVALID_SOCKET) {
		log("Failed creating socket", true);
	}
	else {
		log("Socket initialized", true);
	}
	/*memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);
	serv_addr.sin_addr.s_addr = inet_addr("172.217.17.86");
	log("target ip: 172.217.17.86");*/
	/*server = net_
	if (server == nullptr) {
		return 0;
	}*/
	//memcpy((char*)&(serv_addr.sin_addr.s_addr), (char*)(server->h_addr), server->h_length);

	if (net_connect(sock_descriptor, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) < 0) {
		log("failed to connect to server");
	}
	else {
		log("socket connected! ");
		net_write(sock_descriptor, header.c_str(), sizeof(header.c_str()));

		auto fileDownloadPath = (getWorkingPath() + std::string("/pmstore/") + filename);
		log("file download to: " + fileDownloadPath);
		FILE* filehandle = fopen(fileDownloadPath.c_str(), "wb+");

		int readBytes = 0;
		while (true) {
			memset(recvBuff, 0, 1024);
			if ((readBytes = read(sock_descriptor, recvBuff, sizeof(recvBuff) - 1)) > 0) {
				fwrite(recvBuff, sizeof(recvBuff), 1, filehandle);
			}
			else break;
		}
		std::fclose(filehandle);

		log("download finished!");
	}
	
	return 0;
}

int main() {

	std::string filename;
	char filePath[MAXPATHLEN * 2];
	int pathLen;

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	INPUT_Init();

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	bbaConfig();

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb);

	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * 2);
	setvbuf(stdout, NULL, _IONBF, 0);


	log("Initialising FS...");
	if (fatInitDefault()) {
		log("FS OK!", true);
	}
	else {
		log("FS FAIL!", true);
		Stop();
	}
	std::vector<std::string> extensionList;
	extensionList.push_back(".dol");
	extensionList.push_back(".argv");
	bool doOnce = false;
	log("setup complete", true);
	while (1) {
		std::string url = "https://i.ytimg.com/vi/HevHbYbnLHQ/hqdefault.jpg";
		downloadFile(url, "test.jpg");
		log("displaying file browser", true);
		filename = browseForFile(extensionList);
		log("get cwd");
		// Construct a command line
		getcwd(filePath, MAXPATHLEN);
		pathLen = strlen(filePath);
		std::vector<char*> argarray;

		if (strcasecmp(filename.c_str() + filename.size() - 5, ".argv") == 0) {

			FILE* argfile = std::fopen(filename.c_str(), "rb");
			char str[MAXPATHLEN], * pstr;
			const char seps[] = "\n\r\t ";

			while (fgets(str, MAXPATHLEN, argfile)) {
				// Find comment and end string there
				if ((pstr = strchr(str, '#')))
					*pstr = '\0';

				// Tokenize arguments
				pstr = strtok(str, seps);

				while (pstr != NULL) {
					argarray.push_back(strdup(pstr));
					pstr = strtok(NULL, seps);
				}
			}
			std::fclose(argfile);
			filename = argarray.at(0);
		}
		else {
			argarray.push_back(strdup(filename.c_str()));
		}

		if (strcasecmp(filename.c_str() + filename.size() - 4, ".dol") != 0 || argarray.size() == 0) {
			iprintf("no dol file specified\n");
			log("no dol file specified");
		}
		else {
			char* name = argarray.at(0);
			strcpy(filePath + pathLen, name);
			free(argarray.at(0));
			argarray.at(0) = filePath;
			char formatted[2048];
			sprintf(formatted, "Running %s with %d parameters\n", argarray[0], argarray.size());
			log(formatted);
			int err = runDOL(argarray[0], argarray.size(), (const char**)&argarray[0]);
			memset(formatted, 0, sizeof(char) * 2048);
			sprintf(formatted, "Start failed. Error %d\n", err);
			log(formatted);
		}

		while (argarray.size() != 0) {
			free(argarray.at(0));
			argarray.erase(argarray.begin());
		}
		u32 held;
		held = INPUT_ButtonsHeld();
		if (held & PAD_BUTTON_X) {
			if (!doOnce) {
				log("hold X, download file call", true);
				doOnce = true;
				std::string url = "https://i.ytimg.com/vi/HevHbYbnLHQ/hqdefault.jpg";
				downloadFile(url, "test.jpg");
			}
		}
		do {
			INPUT_Scan();

			VIDEO_WaitVSync();
		} while (held & PAD_BUTTON_START);

	}
	exitLog();
}
