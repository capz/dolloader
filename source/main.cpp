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
#include "dns.h"
#define SCREEN_COLS 70
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 5
#define ENTRIES_START_COLUMN 5
#define ENTRY_PAGE_LENGTH 20
#define MAXPATHLEN 512
#define ONE_MEGABYTE 1048576
#define FILE_DOWNLOAD_BUFFER_SIZE (ONE_MEGABYTE * 4)

static void* xfb = NULL;

u32 first_frame = 1;
GXRModeObj* rmode;

FILE* logFileHandle = nullptr;
#define LOGBUFFERSIZE 10
std::string logbuffer[LOGBUFFERSIZE];
int logEntries = 0;

void WaitForAButton();
void writeBufferToFile(char* buffer, size_t bufferSize, std::string destinationPath);

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
//int countOccurrencesInString(std::string s, char charToLookFor) {
//	int count = 0;
//	for (int i = 0; i < s.size(); i++)
//		if (s[i] == charToLookFor) ++count;
//	return count;
//}
void log(std::string message, bool pause = false) {
	/*if ((logEntries % LOGBUFFERSIZE) == (LOGBUFFERSIZE - 1) || directWriteMode) {
		log_flush();
	}*/
	
	time_t now = time(0);

	// convert now to string form
	std::string timestring(ctime(&now));
	logbuffer[logEntries++ % LOGBUFFERSIZE] = std::move(timestring.substr(0, timestring.length() - 1) + " | " + message + '\0');
	logbuffer[logEntries+1 % LOGBUFFERSIZE] = "" + '\0';

	std::printf("\x1b[2J");
	int i = 0;
	for (auto entry : logbuffer) {
		if (entry.length() < 1) continue;
		std::printf("\x1b[%d;%dH", i + ENTRIES_START_ROW, ENTRIES_START_COLUMN);
		std::printf("%s", entry.c_str());
		++i;
		//fwrite(m.c_str(), sizeof(char), sizeof(char) * m.length(), logFileHandle);
	}
	if (pause) WaitForAButton();
}

void WaitForAButton() {
	while (1) {
		VIDEO_WaitVSync();
		INPUT_Scan();

		if (INPUT_ButtonsDown() & PAD_BUTTON_A) {
			return;
		}
	}
}

void Stop() {
	while (1) {

		VIDEO_WaitVSync();
		INPUT_Scan();

		int buttonsDown = INPUT_ButtonsDown();

		if (buttonsDown & PAD_TRIGGER_Z) {
			break;
		}
	}
}
std::string extractHostnameFromString(std::string url) {
	const auto domainStart = url.find_first_of(':') + 3;
	const auto domainEnd = url.find_first_of('/', domainStart + 1);
	return url.substr(domainStart, domainEnd - domainStart);
}
void bbaConfig() {
	char localip[16] = { 0 };
	char gateway[16] = { 0 };
	char netmask[16] = { 0 };
	s32 ret = if_config(localip, netmask, gateway, TRUE, 20);
	if (ret >= 0) {
		log("bba setup success! \nip: [" + std::string(localip) + "]\ngateway: [" + std::string(gateway) + "]\nnetmask: [" + std::string(netmask) + "]");
	}
	else {
		log("bba setup failed", true);
	}
	auto configPath = (getWorkingPath() + "/resolv.conf");
	log("setting dns servers from config [" + configPath + "]");
	pm_getDnsServers(configPath.c_str());
	for (auto dnsEntry : dns_servers) {
		auto dns = std::string(dnsEntry);
		if(!dns.empty()) log(dns);
	}
	//strcpy(dns_servers[0], "\0");
	log("done", true);
}
void printAddress(sockaddr_in address) {
	log("Addr family: ");
	switch (address.sin_family) {
	case AF_INET: log("[IP version 4]"); break;
	default:
		log(" - UNSPECIFIED");
			break;
	}
	char ip[30];
	strcpy(ip, (char*)inet_ntoa((struct in_addr)address.sin_addr));
	log(std::string("Addr ip: [") + ip + "]");
	log(std::string("Addr port: [") + std::to_string(address.sin_port) + "]", true);
}
//the headers used have been tested using firefox and work for making the request
int downloadFile(std::string apiEndpoint, std::string filename) {
	log("downloadFile", true);
	int sock_descriptor; // integer number to access socket
	log("endpoint dns lookup...", true);
	auto remoteHostname = extractHostnameFromString(apiEndpoint);
	log("remote hostname: " + remoteHostname, true);
	auto serv_addr = pm_gethostbyname((unsigned char*)remoteHostname.c_str(), T_IPv4ADDRESS);
	printAddress(serv_addr);

	//char recvBuff[FILE_DOWNLOAD_BUFFER_SIZE];  // Receiving buffer
	//std::string header = "GET " + apiEndpoint + " HTTP/1.1"
	//	"User-Agent: PuppetMaster/0.1 (Nintendo Gamecube; PPC32)\n"
	//	"Accept-Encoding: gzip, deflate, br\n"
	//	"Accept-Language: en-GB,en;q=0.8,en-US;q=0.6,da;q=0.4\n"
	//	"Connection: keep-alive\n"
	//	"Upgrade-Insecure-Requests: 1\n"
	//	"Pragma: no-cache\n"
	//	"Cache - Control : no - cache\n"
	//	"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\n\n";

	//sock_descriptor = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // SOCK_STREAM = TCP, AF_INET = DOMAIN
	//log("net_socket", true);

	//if (sock_descriptor == INVALID_SOCKET) {
	//	log("Failed creating socket", true);
	//}
	//else {
	//	log("Socket initialized", true);
	//}

	//if (net_connect(sock_descriptor, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) < 0) {
	//	log("failed to connect to server", true);
	//}
	//else {
	//	log("socket connected! downloading file..", true);

	//	net_write(sock_descriptor, header.c_str(), sizeof(header.c_str()));

	//	int readBytes = 0;
	//	while (true) {
	//		memset(recvBuff, 0, FILE_DOWNLOAD_BUFFER_SIZE);
	//		if ((readBytes = net_read(sock_descriptor, recvBuff, sizeof(recvBuff) - 1)) < 1) {
	//			break;
	//		}
	//	}
	//	net_close(sock_descriptor);
	//	log("download complete!", true);

	//	auto fileDownloadPath = (getWorkingPath() + std::string("/pmstore/") + filename);
	//	writeBufferToFile(recvBuff, readBytes, fileDownloadPath);
	//}
	
	return 0;
}

void writeBufferToFile(char* buffer, size_t bufferSize, std::string destinationPath) {
	log("opened file", true);
	FILE* filehandle = fopen(destinationPath.c_str(), "wb+");
	log("writing data...", true);
	fwrite(buffer, bufferSize, 1, filehandle);
	log("done!", true);
	fclose(filehandle);
	filehandle = nullptr;
}
int main() {

	std::string filename;
	char filePath[MAXPATHLEN * 2];
	int pathLen;

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	INPUT_Init();

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

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
		log("FS OK!");
	}
	else {
		log("FS FAIL!", true);
		Stop();
	}
	bbaConfig();
	std::vector<std::string> extensionList;
	extensionList.push_back(".dol");
	extensionList.push_back(".argv");
	bool doOnce = false;
	log("setup complete", true);
	std::string url = "https://i.ytimg.com/vi/HevHbYbnLHQ/hqdefault.jpg";
	log("attempting download from url: [" + url + "]");
	downloadFile(url, "test.jpg");
	//while (1) {
	//	log("displaying file browser", true);
	//	filename = browseForFile(extensionList);
	//	log("get cwd");
	//	// Construct a command line
	//	getcwd(filePath, MAXPATHLEN);
	//	pathLen = strlen(filePath);
	//	std::vector<char*> argarray;

	//	if (strcasecmp(filename.c_str() + filename.size() - 5, ".argv") == 0) {

	//		FILE* argfile = std::fopen(filename.c_str(), "rb");
	//		char str[MAXPATHLEN], * pstr;
	//		const char seps[] = "\n\r\t ";

	//		while (fgets(str, MAXPATHLEN, argfile)) {
	//			// Find comment and end string there
	//			if ((pstr = strchr(str, '#')))
	//				*pstr = '\0';

	//			// Tokenize arguments
	//			pstr = strtok(str, seps);

	//			while (pstr != NULL) {
	//				argarray.push_back(strdup(pstr));
	//				pstr = strtok(NULL, seps);
	//			}
	//		}
	//		std::fclose(argfile);
	//		filename = argarray.at(0);
	//	}
	//	else {
	//		argarray.push_back(strdup(filename.c_str()));
	//	}

	//	if (strcasecmp(filename.c_str() + filename.size() - 4, ".dol") != 0 || argarray.size() == 0) {
	//		iprintf("no dol file specified\n");
	//		log("no dol file specified");
	//	}
	//	else {
	//		char* name = argarray.at(0);
	//		strcpy(filePath + pathLen, name);
	//		free(argarray.at(0));
	//		argarray.at(0) = filePath;
	//		char formatted[2048];
	//		sprintf(formatted, "Running %s with %d parameters\n", argarray[0], argarray.size());
	//		log(formatted);
	//		int err = runDOL(argarray[0], argarray.size(), (const char**)&argarray[0]);
	//		memset(formatted, 0, sizeof(char) * 2048);
	//		sprintf(formatted, "Start failed. Error %d\n", err);
	//		log(formatted);
	//	}

	//	while (argarray.size() != 0) {
	//		free(argarray.at(0));
	//		argarray.erase(argarray.begin());
	//	}
	//	u32 held;
	//	held = INPUT_ButtonsHeld();
	//	if (held & PAD_BUTTON_X) {
	//		if (!doOnce) {
	//			log("hold X, download file call", true);
	//			doOnce = true;
	//			std::string url = "https://i.ytimg.com/vi/HevHbYbnLHQ/hqdefault.jpg";
	//			downloadFile(url, "test.jpg");
	//		}
	//	}
	//	do {
	//		INPUT_Scan();

	//		VIDEO_WaitVSync();
	//	} while (held & PAD_BUTTON_START);

	//}
	exitLog();
}
