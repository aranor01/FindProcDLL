#include "NSIS.h"
#include <VersionHelpers.h>

#pragma comment(linker, "/MERGE:.rdata=.text")
#pragma comment(linker, "/SECTION:.text,RE")

HINSTANCE g_hInstance;
HWND g_hwndParent;

#ifdef UNICODE
#define API_AW(X) X "W"
#define MAX_LEN 1024
#else
#define API_AW(X) X "A"
#define MAX_LEN 8192
#endif

#define FINDPROC 0
#define KILLPROC 1
#define WAITPROCEND 2
#define WAITPROCSTART 3

// cleanup functions
HINSTANCE hInstLib = 0, hKernel32 = 0;
HANDLE hProc = 0;

void Cleanup()
{
	//hInstLib ? FreeLibrary(hInstLib) : 0; 
	//hInstLib = 0;
	//hKernel32 ? FreeLibrary(hKernel32) : 0; 
	//hKernel32 = 0;
	hProc ? CloseHandle(hProc) : 0;
	hProc = 0;
}

#define RETURN_CODE(X) {lstrcpy((TCHAR*) szToFind,__T(#X)); Cleanup(); return;}


void FindProcByName(const TCHAR* szToFind, int operation, int timeout)
// Created: 12/29/2000  (RK)
// Last modified: 2/06/2011  (RK)
// Please report any problems or bugs to kochhar@physiology.wisc.edu (no longer this version, mostly rewritten)
// The latest version of this routine can be found at:
//     http://www.neurophys.wisc.edu/ravi/software/killproc/
// Check whether the process "szToFind" is currently running in memory
// This works for Win/95/98/ME and also Win/NT/2000/XP
// The process name is case-insensitive, i.e. "notepad.exe" and "NOTEPAD.EXE"
// will both work (for szToFind)
// Return codes are as follows:
//   0   = Process was not found
//   1   = Process was found / Wait success
//   605 = Unable to search for process
//   606 = Unable to identify system type
//   607 = Unsupported OS
//   100 = WaitForEnd - WAIT_TIMEOUT (reached timeout)
// Change history:
//  3/10/2002   - Fixed memory leak in some cases (hSnapShot and
//                and hSnapShotm were not being closed sometimes)
//  6/13/2003   - Removed iFound (was not being used, as pointed out
//                by John Emmas)
//  06/02/2011  - mostly rewritten, added support for kill/wait and 64bit, removed support for Win 9x (sorry bro); by hnedka
{
	// initializations
	DWORD aiPID[8192], iNumProc;
	DWORD iCbneeded;
	TCHAR szPath[MAX_LEN], szTemp[MAX_LEN], szTemp2[MAX_LEN];
	OSVERSIONINFO osvi;
	HMODULE hMod;
	int len1, len2;
	DWORD i, j;
	DWORD tmp1, tmp2, tmp3;
	DWORD openprocess_rights;
	int processed = 0, processed2 = 0;

	// PSAPI Function Pointers.
	BOOL(WINAPI * lpfEnumProcesses) (DWORD*, DWORD cb, DWORD*);
	BOOL(WINAPI * lpfEnumProcessModules) (HANDLE, HMODULE*, DWORD, LPDWORD) = 0;
	DWORD(WINAPI * lpfGetModuleFileNameEx) (HANDLE, HMODULE, LPTSTR, DWORD) = 0;
	DWORD(WINAPI * lpGetProcessImageFileName) (HANDLE, LPTSTR, DWORD) = 0;
	BOOL(WINAPI * lpQueryFullProcessImageName) (HANDLE, DWORD, LPTSTR, DWORD*);

	// *** First check what version of Windows we're in ***
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// Working on NT/2000/XP/Vista/7
	// load libraries (if not already loaded) and functions; if EnumProcesses found in kernel32 (Win 7, new builds+), then don't load PSAPI.dll
	hKernel32 = hKernel32 ? hKernel32 : LoadLibrary(_T("kernel32.dll"));

	lpfEnumProcesses = (BOOL(WINAPI*)(DWORD*, DWORD, DWORD*)) GetProcAddress(hKernel32, "EnumProcesses");

	if (!lpfEnumProcesses)
	{
		hInstLib = hInstLib ? hInstLib : LoadLibrary(_T("PSAPI.DLL"));
		lpfEnumProcesses = (BOOL(WINAPI*)(DWORD*, DWORD, DWORD*)) GetProcAddress(hInstLib, "EnumProcesses");
	}

	if (!lpfEnumProcesses)
		RETURN_CODE(605);

	lpQueryFullProcessImageName = (BOOL(WINAPI*)(HANDLE, DWORD, LPTSTR, DWORD*)) GetProcAddress(hKernel32, API_AW("QueryFullProcessImageName"));

	if (!lpQueryFullProcessImageName)
	{
		lpfEnumProcessModules = (BOOL(WINAPI*)(HANDLE, HMODULE*, DWORD, LPDWORD)) GetProcAddress(hInstLib, "EnumProcessModules");
		lpfGetModuleFileNameEx = (DWORD(WINAPI*)(HANDLE, HMODULE, LPTSTR, DWORD)) GetProcAddress(hInstLib, API_AW("GetModuleFileNameEx"));
		lpGetProcessImageFileName = (DWORD(WINAPI*)(HANDLE, LPTSTR, DWORD)) GetProcAddress(hInstLib, API_AW("GetProcessImageFileName"));
	}

	// determine which process rights to use
	openprocess_rights = lpQueryFullProcessImageName ? PROCESS_QUERY_LIMITED_INFORMATION : (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
	openprocess_rights |= (operation == KILLPROC) ? PROCESS_TERMINATE : 0;
	openprocess_rights |= (operation == WAITPROCEND) ? SYNCHRONIZE : 0;

	// get process list
restart_search:
	if (!lpfEnumProcesses(aiPID, sizeof(aiPID), &iCbneeded))
		RETURN_CODE(605);

	// How many processes are there?
	iNumProc = iCbneeded / sizeof(DWORD);

	// Get and match the name of each process
	for (i = 0; i < iNumProc; i++)
	{
		// skip current process ID
		if (aiPID[i] == GetCurrentProcessId())
			continue;

		// open process
		hProc = OpenProcess(openprocess_rights, 0, aiPID[i]);

		if (!hProc)
			continue;

		// Win Vista/7 - first way (QueryFullProcessImageName)
		if (lpQueryFullProcessImageName)
		{
			tmp1 = MAX_LEN;

			if (!lpQueryFullProcessImageName(hProc, 0, szPath, &tmp1))
			{
				DWORD d = GetLastError();
				wsprintf(szPath, _T("Err: %d ID: %d \n"), d, aiPID[i]);
				OutputDebugString(szPath);
				goto loop_continue;
			}
		}

		// Win 2000/XP
		else
		{
			if (!lpfEnumProcessModules || !lpfGetModuleFileNameEx)
				RETURN_CODE(605);

			// 2000 / XP-32 - second way (fGetModuleFileNameEx)
			if (!lpfEnumProcessModules(hProc, &hMod, sizeof(hMod), &iCbneeded) || !lpfGetModuleFileNameEx(hProc, hMod, szPath, MAX_LEN))

				// XP-64 - third way (GetProcessImageFileName)
			{
				if (!lpGetProcessImageFileName)
					RETURN_CODE(605);

				if (!lpGetProcessImageFileName(hProc, szTemp, MAX_LEN))
					goto loop_continue;

				// convert path from the kernel format (\Device\HardDisk0\...) to the standard format (C:\...):
				// get the drive part
				tmp1 = 0;
				tmp2 = lstrlen(szTemp);

				for (j = 0; j < tmp2; j++)
				{
					if (szTemp[j] == __T('\\'))
						tmp1++;

					if (tmp1 == 3)
					{
						szTemp[j] = 0;
						break;
					}
				}

				// get list of all drives in the standard format
				tmp1 = GetLogicalDriveStrings(MAX_LEN - 1, szTemp2);

				if (!tmp1 || tmp1 > MAX_LEN - 1)
					goto loop_continue;

				// convert standard drives one by one and compare with our kernel drive
				tmp2 = 0;

				do
				{
					tmp3 = lstrlen(szTemp2 + tmp2);

					if (szTemp2[tmp2 + tmp3 - 1] == __T('\\'))
						szTemp2[tmp2 + tmp3 - 1] = 0;

					QueryDosDevice(szTemp2 + tmp2, szPath, MAX_PATH);

					if (!lstrcmpi(szPath, szTemp))
					{
						lstrcpy(szPath, szTemp2 + tmp2);
						szPath[tmp3 - 1] = __T('\\');
						lstrcpy(szPath + tmp3, szTemp + lstrlen(szTemp) + 1);
						break;
					}

					tmp2 += tmp3 + 1;

				} while (tmp2 < tmp1);
			}
		}

		// convert to long path
		GetLongPathName(szPath, szPath, MAX_LEN);

		OutputDebugString(szPath);
		OutputDebugString(_T("\n"));

		len1 = lstrlen(szToFind);
		len2 = lstrlen(szPath);

		// same length - just compare
		if (len1 == len2)
			tmp1 = lstrcmpi(szPath, szToFind);

		// to find string begins with letter - compare path beginning
		else if (len1 < len2 && len1 >= 3 && szToFind[1] == __T(':'))
		{
			szPath[len1] = 0;
			tmp1 = lstrcmpi(szPath, szToFind);
		}

		// compare path end
		else if (len1 < len2 && szPath[len2 - len1 - 1] == __T('\\'))
			tmp1 = lstrcmpi(szPath + len2 - len1, szToFind);

		// error
		else
			tmp1 = 1;

		if (tmp1)
			goto loop_continue;

		// findproc
		if (operation == FINDPROC || operation == WAITPROCSTART)
		{
			RETURN_CODE(1);
		}

		// killproc
		else if (operation == KILLPROC)
		{
			if (TerminateProcess(hProc, 0))
				processed++;
		}

		// waitproc
		else if (operation == WAITPROCEND)
		{
			tmp1 = WaitForSingleObject(hProc, timeout);

			if (tmp1 == WAIT_TIMEOUT)
				RETURN_CODE(100)

				// if somehow still running (either WaitForSingleObject failed or it returned STILL_ACTIVE as the exit code),
				// we Sleep a little bit and then continue (the procedure is restarted)
				if (GetExitCodeProcess(hProc, &tmp1) == STILL_ACTIVE)
					Sleep(250);

			processed++;
		}

	loop_continue:
		CloseHandle(hProc);
		hProc = 0;
	}

	if (operation == KILLPROC)
	{
		if (processed)
			RETURN_CODE(1)
		else
			RETURN_CODE(0);
	}

	else if (operation == WAITPROCEND && processed)
	{
		if (processed)
		{
			processed2 += processed;
			processed = 0;
			goto restart_search;
		}

		else if (processed2)
			RETURN_CODE(1)

		else
			RETURN_CODE(0)
	}

	else if (operation == WAITPROCSTART)
	{
		Sleep(timeout);
		goto restart_search;
	}

	RETURN_CODE(0);
}

//
// This is the only exported function, FindProc. It receives the name
// of a process through the NSIS stack. The return-value from the
// FindProcByName function is stored in the $R0 variable, so push
// it before calling FindProc function if you don't want to lose the
// data that R0 could contain.
//
// You can call this function in NSIS like this:
//
// FindProcDLL::FindProc "process_name.exe"
//
// example:
//    FindProcDLL::FindProc "msnmsgr.exe"
//  would return 1 in R0 if running
//  if it's not running, it would return 0.
//
//  ---------------------------

// KillProc and WaitProcStart/WaitProcEnd work in a similar way

int custom_atoi(TCHAR* number)
{
	int i;
	int result = 0;
	int len = lstrlen(number);

	if (number[0] == __T('-'))
		return INFINITE;

	for (i = 0; i < len; i++)
	{
		result *= 10;
		result += number[i] - __T('0');
	}

	return result;
}

void FindProcWrap(HWND hwndParent, int string_size, TCHAR* variables, stack_t** stacktop, int operation)
{
	TCHAR parameter[200];
	TCHAR timeout_str[200];
	int timeout = 0;
	g_hwndParent = hwndParent;

	EXDLL_INIT();
	{
		popstring(parameter);

		if (operation == WAITPROCSTART || operation == WAITPROCEND)
		{
			popstring(timeout_str);
			timeout = custom_atoi(timeout_str);
		}

		FindProcByName(parameter, operation, timeout);
		setuservariable(INST_R0, parameter);
	}
}

__declspec(dllexport) void FindProc(HWND hwndParent, int string_size, TCHAR* variables, stack_t** stacktop)
{
	FindProcWrap(hwndParent, string_size, variables, stacktop, FINDPROC);
}

__declspec(dllexport) void KillProc(HWND hwndParent, int string_size, TCHAR* variables, stack_t** stacktop)
{
	FindProcWrap(hwndParent, string_size, variables, stacktop, KILLPROC);
}

__declspec(dllexport) void WaitProcEnd(HWND hwndParent, int string_size, TCHAR* variables, stack_t** stacktop)
{
	FindProcWrap(hwndParent, string_size, variables, stacktop, WAITPROCEND);
}

__declspec(dllexport) void WaitProcStart(HWND hwndParent, int string_size, TCHAR* variables, stack_t** stacktop)
{
	FindProcWrap(hwndParent, string_size, variables, stacktop, WAITPROCSTART);
}
