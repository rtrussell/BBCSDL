/******************************************************************\
*	  BBC BASIC for SDL 2.0 (Windows edition)		   *
*	  Copyright (c) R. T. Russell, 2024			   *
*								   *
*	  Version 1.4, 15-Nov-2024				   *
\******************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <shlobj.h>
#include "version.h"

#ifdef __x86_64__
#define SECURITY_PTR 0x128
#else
#define SECURITY_PTR 0x118
#endif
#define STACK_NEEDED 0x400

	// Functions in BBCUPCK.NAS

void WINAPI unpack (HANDLE, char*, int, DWORD*, int) ;
BOOL WINAPI verify (HANDLE, char*, int) ;

	// Structure declaration

typedef struct tagPROGINFO {
  int DefaultRAM ;
  int Library ;
  int WindowWidth ;
  int WindowHeight ;
  int CmdShow ;
  int Flags ;
  unsigned int SerialNumber ;
  int ProgLength ;
} PROGINFO, *LPPROGINFO ;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
		    PSTR szCmdLine, int iCmdShow)
    {
	char szAppName[_MAX_PATH] ;
	char szLoadDir[_MAX_PATH] ;
	char szTempDir[_MAX_PATH] ;
	char szLibrary[_MAX_PATH] ;
	char szBinaries[_MAX_PATH] ;
	char szFileName[_MAX_PATH] ;
	char szEmptyFile[_MAX_PATH] ;
	PROGINFO ProgInfo ;
	SHFILEOPSTRUCT shfo = {0} ;
	SHELLEXECUTEINFOA sei = {0} ;
	HANDLE ThisFile, EmbedFile ;
	int signature, offset ;
	int embedsize, remain, dirlen, liblen ;
	BOOL anyresources = 0 ;
	DWORD nRead ;
	char *nameptr ;
	void *pBuffer ;

	// Get the path to this file and open it:
	GetModuleFileName (NULL, szFileName, _MAX_PATH) ;

	ThisFile = CreateFile (szFileName, GENERIC_READ, FILE_SHARE_READ,
			    NULL, OPEN_EXISTING, 0, NULL) ;

	// It shouldn't be possible for the file not to exist:
	if (ThisFile == INVALID_HANDLE_VALUE)
	  return 1;

	// Discover if there is a signature and if so where it is:
	SetFilePointer (ThisFile, SECURITY_PTR, NULL, FILE_BEGIN) ;
	ReadFile (ThisFile, &signature, sizeof(int), &nRead, NULL) ;
	if (signature != 0)
		nRead = FILE_BEGIN ;
	else
		nRead = FILE_END ;

	// Read the ProgInfo structure either from the end or just before the signature:
	offset = SetFilePointer (ThisFile, signature-sizeof(PROGINFO), NULL, nRead) ;
	ReadFile (ThisFile, &ProgInfo, sizeof(PROGINFO), &nRead, NULL) ;

	// Quit if there are no embedded files:
	if ((ProgInfo.Library <= 0) || (ProgInfo.Library >= offset))
	    {
		CloseHandle (ThisFile) ;
		return 0 ;
	    }

	// Find directory containing this file:
	GetFullPathName (szFileName, _MAX_PATH, szLoadDir, &nameptr) ;
	*nameptr = '\0' ;
	dirlen = strlen (szLoadDir) ;

	// Get the path to the temporary directory:
	GetTempPath (_MAX_PATH, szTempDir) ;

	// Set the path to the binaries subdirectory and create it:
	GetTempFileName (szTempDir, "BBC", 0, szBinaries) ;
	DeleteFile (szBinaries) ;
	CreateDirectory (szBinaries, NULL) ;
	strcat (szBinaries, "\\") ;

	// Set the path to the libraries subdirectory (but don't create it):
	strcpy (szLibrary, szBinaries) ;
	strcat (szLibrary, "lib\\") ;
	liblen = strlen (szLibrary) ;

	// Set the path to an empty file bin\$ (but don't create it):
	strcpy (szEmptyFile, szBinaries) ;
	strcat (szEmptyFile, "$") ; 

	// Allocate a buffer to hold the uncompressed data:
	pBuffer = malloc (ProgInfo.DefaultRAM) ;
	if (pBuffer == NULL)
		return 2 ;

	// For each of the embedded files:
	int embedptr = offset - ProgInfo.ProgLength - ProgInfo.Library ;
	while (embedptr < (offset - ProgInfo.ProgLength)) 
	    {
		int len ;
		char type ;
		SetFilePointer (ThisFile, embedptr, NULL, FILE_BEGIN) ;
		ReadFile (ThisFile, &type, 1, &nRead, NULL) ;
		ReadFile (ThisFile, &embedsize, 4, &nRead, NULL) ;
		ReadFile (ThisFile, szFileName, _MAX_PATH, &nRead, NULL) ;
		len = strlen(szFileName) ;
		embedptr += len + 6 ;

		// type == 0/1 for @dir$, type == 2/3 for @dir$\bin\lib
		if (type & 2)
		    {
			memmove (szFileName + liblen, szFileName, len + 1) ;
			memcpy (szFileName, szLibrary, liblen) ;
		    }
		else
		    {
			memmove (szFileName + dirlen, szFileName, len + 1) ;
			memcpy (szFileName, szLoadDir, dirlen) ;
			anyresources = 1 ;
		    }

		// Check if the file already exists:
		EmbedFile = CreateFile (szFileName, GENERIC_READ,
			  FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL) ;

		// If it does, verify the existing contents against the embedded data:
		if (EmbedFile != INVALID_HANDLE_VALUE)
		    {
			int diff = 0 ;
			SetFilePointer (ThisFile, embedptr, NULL, FILE_BEGIN) ;

			while ((diff == 0) && ((remain = embedptr + embedsize -
				SetFilePointer (ThisFile, 0, NULL, FILE_CURRENT)) > 0))
			    {
				nRead = ProgInfo.SerialNumber ;
				unpack (ThisFile, pBuffer, min(remain, ProgInfo.DefaultRAM),
					 &nRead, type) ;
				diff = verify (EmbedFile, pBuffer, nRead) ;
			    }
			CloseHandle (EmbedFile) ;
			if (diff)
				EmbedFile = INVALID_HANDLE_VALUE ;
		    }

		// If the file doesn't exist OR is different from the existing one, create it:
		if (EmbedFile == INVALID_HANDLE_VALUE)
		    {
			CloseHandle (CreateFile (szEmptyFile, GENERIC_WRITE, 0,
						 NULL, CREATE_ALWAYS, 0, NULL)) ;
			
			shfo.hwnd = (HWND) 1 ; // Any non-zero value!
			shfo.wFunc = FO_MOVE ;
			shfo.pFrom = szEmptyFile ;
			shfo.pTo   = szFileName ;
			shfo.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR ;
			*(szFileName + strlen(szFileName) + 1) = '\0' ;
			*(szEmptyFile + strlen(szEmptyFile) + 1) = '\0' ;
			SHFileOperation (&shfo) ; // Create directory if necessary

			EmbedFile = CreateFile (szFileName, GENERIC_WRITE, 0,
					   NULL, CREATE_ALWAYS, 0, NULL) ;

			if (EmbedFile == INVALID_HANDLE_VALUE)
			    {
				MessageBox(NULL, szFileName, "SDLRUN: Cannot create file", 0);
				return 3 ;
			    }

			SetFilePointer (ThisFile, embedptr, NULL, FILE_BEGIN) ;

			while ((remain = embedptr + embedsize -
				SetFilePointer (ThisFile, 0, NULL, FILE_CURRENT)) > 0)
			    {
				nRead = ProgInfo.SerialNumber ;
				unpack (ThisFile, pBuffer, min(remain, ProgInfo.DefaultRAM),
					&nRead, type) ;
				WriteFile (EmbedFile, pBuffer, nRead, &nRead, NULL) ;
			    }

			CloseHandle (EmbedFile) ;
		    }
		embedptr += embedsize ;
	    }

	free (pBuffer) ;
	ReadFile (ThisFile, szAppName, _MAX_PATH, &nRead, NULL) ;
	CloseHandle (ThisFile) ;

	// Get path to program .bbc file and hide it:
	if (anyresources)
		strcpy (szFileName, szLoadDir) ;
	else
		strcpy (szFileName, szBinaries) ;
	strcat (szFileName, szAppName) ;
	strcat (szFileName, ".bbc") ;
	SetFileAttributes (szFileName, FILE_ATTRIBUTE_HIDDEN) ;

	// Enclose in quotes in case embedded space(s):
	strcpy (szEmptyFile, "\042") ;
	strcat (szEmptyFile, szFileName) ;
	strcat (szEmptyFile, "\042 ") ;
	strcat (szEmptyFile, szCmdLine) ;

	// Run BBCSDL:
	strcpy (szLibrary, szBinaries) ;
	strcat (szLibrary, "bbcsdl.exe") ;

	/// ShellExecute ((HWND) 0, "open", szLibrary, szEmptyFile, szLoadDir, ProgInfo.CmdShow) ;

	sei.cbSize = sizeof(sei) ;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS ;
        sei.hwnd = NULL ;
	sei.lpVerb = "open" ;
        sei.lpFile = szLibrary ;
        sei.lpParameters = szEmptyFile ;
        sei.lpDirectory = szLoadDir ;
        sei.nShow = ProgInfo.CmdShow ;
        ShellExecuteEx (&sei) ;

	// Wait for process to finish:
        if (sei.hProcess)
	    {
		DWORD res ;
		do res = WaitForSingleObject (sei.hProcess, 1000) ;
		while (res == WAIT_TIMEOUT) ;
		CloseHandle (sei.hProcess) ;
	    }

	// Delete the binaries directory and all the files it contains:
	*(szBinaries + strlen(szBinaries) + 1) = '\0' ;
	shfo.hwnd = (HWND) 1 ; // Any non-zero value!
	shfo.pFrom = szBinaries ;
	shfo.wFunc = FO_DELETE ;
	shfo.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR ;
	SHFileOperation (&shfo) ; 

	return 0 ;
    }
