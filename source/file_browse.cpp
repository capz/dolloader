#include "file_browse.h"
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include "input.h"

#define PATH_LEN 1024

#define SCREEN_COLS 70
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 5
#define ENTRIES_START_COLUMN 5
#define ENTRY_PAGE_LENGTH 10

using namespace std;

struct DirEntry {
	string name;
	bool isDirectory;
} ;

bool nameEndsWith (const string& name, const vector<string> extensionList) {

	if (name.size() == 0) return false;

	if (extensionList.size() == 0) return true;

	for (unsigned int i = 0; i < extensionList.size(); i++) {
		const string ext = extensionList.at(i);
		if ( strcasecmp (name.c_str() + name.size() - ext.size(), ext.c_str()) == 0) return true;
	}
	return false;
}


bool dirEntryPredicate (const DirEntry& lhs, const DirEntry& rhs) {
	if (!lhs.isDirectory && rhs.isDirectory) {
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory) {
		return true;
	}
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

void getDirectoryContents (vector<DirEntry>& dirContents, const vector<string> extensionList) {
	struct stat st;

	dirContents.clear();

	DIR *pdir = opendir ("."); 
	
	if (pdir == NULL) {
		printf ("Unable to open the directory.\n");
	} else {

		while(true) {
			DirEntry dirEntry;

			struct dirent* pent = readdir(pdir);
			if(pent == NULL) break;
				
			stat(pent->d_name, &st);
			dirEntry.name = pent->d_name;
			dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;

			if (dirEntry.name.compare(".") != 0 && (dirEntry.isDirectory || nameEndsWith(dirEntry.name, extensionList))) {
				dirContents.push_back (dirEntry);
			}

		}
		
		closedir(pdir);
	}	
	
	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);
}

void getDirectoryContents (vector<DirEntry>& dirContents) {
	vector<string> extensionList;
	getDirectoryContents (dirContents, extensionList);
}

void showDirectoryContents (const vector<DirEntry>& dirContents, int startRow) {
	char path[PATH_LEN];
	
	
	getcwd(path, PATH_LEN);
	
	// Clear the screen
	printf ("\x1b[2J");
	
	// Move to 2nd row
	printf ("\x1b[%d;%dH", ENTRIES_START_ROW - 2, ENTRIES_START_COLUMN );
	// Print the path
	if (strlen(path) < SCREEN_COLS) {
		printf ("%s", path);
	} else {
		printf ("%s", path + strlen(path) - SCREEN_COLS);
	}
	
	// Move to 2nd row
	printf ("\x1b[%d;%dH", ENTRIES_START_ROW - 1, ENTRIES_START_COLUMN);
	// Print line of dashes
	printf ("--------------------------------");
	
	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		const DirEntry* entry = &dirContents.at(i + startRow);
		char entryName[SCREEN_COLS + 1];
		
		// Set row
		printf ("\x1b[%d;%dH", i + ENTRIES_START_ROW, ENTRIES_START_COLUMN);
		
		if (entry->isDirectory) {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 3] = '\0';
			printf (" [%s]", entryName);
		} else {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 1] = '\0';
			printf (" %s", entryName);
		}
	}
}

string browseForFile (const vector<string> extensionList) {
	int pressed = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	vector<DirEntry> dirContents;
	
	getDirectoryContents (dirContents, extensionList);
	showDirectoryContents (dirContents, screenOffset);
	
	while (true) {
		// Clear old cursors
		for (int i = ENTRIES_START_ROW; i < ENTRIES_PER_SCREEN + ENTRIES_START_ROW; i++) {
			printf ("\x1b[%d;%dH ", i, ENTRIES_START_COLUMN);
		}
		// Show cursor
		printf ("\x1b[%d;%dH*", fileOffset - screenOffset + ENTRIES_START_ROW, ENTRIES_START_COLUMN);
		
		do {
			INPUT_Scan();

			pressed = INPUT_ButtonsDown();
			VIDEO_WaitVSync();
		} while (!pressed);
	
		if (pressed & BUTTON_UP) 	fileOffset -= 1;
		if (pressed & BUTTON_DOWN) 	fileOffset += 1;
		if (pressed & BUTTON_LEFT) 	fileOffset -= ENTRY_PAGE_LENGTH;
		if (pressed & BUTTON_RIGHT)	fileOffset += ENTRY_PAGE_LENGTH;
		
		if (fileOffset < 0) 	fileOffset = dirContents.size() - 1;		// Wrap around to bottom of list
		if (fileOffset > ((int)dirContents.size() - 1))		fileOffset = 0;		// Wrap around to top of list

		// Scroll screen if needed
		if (fileOffset < screenOffset) 	{
			screenOffset = fileOffset;
			showDirectoryContents (dirContents, screenOffset);
		}
		if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
			screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
			showDirectoryContents (dirContents, screenOffset);
		}
		
		if (pressed & BUTTON_SELECT) {
			DirEntry* entry = &dirContents.at(fileOffset);
			if (entry->isDirectory) {
				// Enter selected directory
				chdir (entry->name.c_str());
				getDirectoryContents (dirContents, extensionList);
				screenOffset = 0;
				fileOffset = 0;
				showDirectoryContents (dirContents, screenOffset);
			} else {
				// Clear the screen
				printf ("\x1b[2J");
				// Return the chosen file
				return entry->name;
			}
		}
		
		if (pressed & BUTTON_BACK) {
			// Go up a directory
			chdir ("..");
			getDirectoryContents (dirContents, extensionList);
			screenOffset = 0;
			fileOffset = 0;
			showDirectoryContents (dirContents, screenOffset);
		}
		
		if (pressed & BUTTON_REBOOT ) exit(0);
	}
}
