# FindProcDLL

Original project by iceman_k (Sunil Kamath), 2003: https://nsis.sourceforge.io/Find_Process_By_Name  
based upon the FIND_PROC_BY_NAME function written by Ravi Kochhar: http://www.physiology.wisc.edu/ravi/

Modified version by hnedka, 2011: http://forums.winamp.com/showpost.php?p=2777719&postcount=7

Modified version by aranor01 (Giuseppe Angelone), 2021: [this](https://github.com/aranor01/FindProcDLL).  
Changes:
- Fixed: only the first 250 processes were enumerated
- It doesn't check for OS version, it just fails with return code 605 if it doesn't work on an old Windows.


## Installation

  Copy the 'FindProcDLL.dll' in your NSIS plugins directory

## Usage

  Call the functions with one of the two suggested syntax in the NSIS documentation, e.g.:

    FindProcDLL::FindProc "process_name.exe"

  Or

     ; Pre 2.0 syntax
    SetOutPath $TEMP
    GetTempFileName $8
    File /oname=$8 FindProcDLL.dll
    Push "process_name.exe"
    CallInstDLL FindProc

  $R0 will then hold the return value.


**To check if a process is running:**

    FindProcDLL::FindProc "process_name.exe"

The return codes are as follows:

- 0   = Process was not found
- 1   = Process was found
- 605 = Unable to search for process
- 606 = Unable to identify system type
- 607 = Unsupported OS
- 632 = Process name is invalid

**To kill a process:**

    FindProcDLL::KillProc "process_name.exe"

The return codes are the same listed for FindProc

**To wait for the process to exit:**

    FindProcDLL::WaitProcEnd "process_name.exe" -1

To wait for the the process to exit with a timeout use a positive value in milliseconds as second parameter, in this case the return code will be 100 if the process is still running.

## Alternatives

  You can use taskkill to kill the process, for instance (Windows Vista and later syntax):

    nsExec::Exec '"$SYSDIR\taskkill.exe" /f /im "process_name.exe"'

## Disclaimer

  I have not tested this plugin (I have tested only FindProc, only for Unicode installers and only with one Windows version), do not use it, or use it at your own risk.

## Copyright (original)

  The original source file for the FIND_PROC_BY_NAME function is included in this distribution. The file name is: exam38.cpp, and it MUST BE in this zip file. Other than that, you may use or modify this source code as you wish in any of your projects. However, you MUST include this file as well as the exam38.cpp file if you are distributing the original or modified source to anyone or anywhere.

## Thanks (original)

  Ravi for the FIND_PROC_BY_NAME function.
  DITMan for his KillProcDLL Manual NSIS Plugin which inspired this plugin.
