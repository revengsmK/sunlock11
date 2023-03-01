#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <conio.h>


BOOL setDebugPriv(void){
	
	HANDLE hToken;
	LUID Luid;
	TOKEN_PRIVILEGES NewState;
	
	if(OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES,&hToken) == FALSE)
		return FALSE;
	
	if(LookupPrivilegeValue(NULL, "SeDebugPrivilege", &Luid) == FALSE)
		return FALSE;
	
	
	NewState.PrivilegeCount = 1;
    NewState.Privileges[0].Luid = Luid;
    NewState.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	 
	
	if(AdjustTokenPrivileges(hToken,FALSE,&NewState,sizeof(NewState),NULL,NULL) == 0)
		return FALSE;
	
	
	return TRUE;
	
}


BOOL IsElevated(void){
	
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
	
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
	{
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof( TOKEN_ELEVATION );
		
        if(GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) 
            fRet = Elevation.TokenIsElevated;
    }
	
    if(hToken){
        CloseHandle(hToken);
    }
	
    return fRet;
}

void KillProcessByName(const char *filename){
	
    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    PROCESSENTRY32 pEntry;
	
    pEntry.dwSize = sizeof(PROCESSENTRY32);
	
    BOOL hRes = Process32First(hSnapShot, &pEntry);
	
    while (hRes)
    {
        if (strcmp(pEntry.szExeFile, filename) == 0)
        {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0,
                                          (DWORD) pEntry.th32ProcessID);
            if (hProcess != NULL)
            {
                TerminateProcess(hProcess, 1);
                CloseHandle(hProcess);
            }
        }
		
        hRes = Process32Next(hSnapShot, &pEntry);
    }
	
    CloseHandle(hSnapShot);
}

// Kills RuntimeBroker.exe process which has SettingsEnvironment.Desktop.dll module loaded

void KillRuntimeBroker(void){
	
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
	HANDLE hSnapshotMod;
	HANDLE hProcess;
	PROCESSENTRY32 pEntry = {0};
	MODULEENTRY32 modEntry = {0};
	BOOL hModRes;
	
	pEntry.dwSize = sizeof(PROCESSENTRY32);
	modEntry.dwSize = sizeof(MODULEENTRY32);
	
	BOOL hRes = Process32First(hSnapshot, &pEntry);
	
	while(hRes)
	{
		
		if(strcmp(pEntry.szExeFile,"RuntimeBroker.exe") == 0)
		{
			hSnapshotMod = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pEntry.th32ProcessID );
			
			hModRes = Module32First(hSnapshotMod, &modEntry);
		
			while(hModRes)
			{
				if(strcmp("SettingsEnvironment.Desktop.dll",modEntry.szModule) == 0)
				{
				  hProcess = OpenProcess(PROCESS_TERMINATE,0,(DWORD)pEntry.th32ProcessID);
				  
				  if(hProcess != NULL)
				  {
					TerminateProcess(hProcess, 1);
					CloseHandle(hProcess);
				  }
				  
				}
				
				hModRes = Module32Next(hSnapshotMod, &modEntry);
			}
			
			CloseHandle(hSnapshotMod);
		}
		
		hRes = Process32Next(hSnapshot, &pEntry);
	}
	
	CloseHandle(hSnapshot);
}

// Simple search for an unmasked hex pattern in a buffer

DWORD seekbytes(unsigned char fileBuff[], unsigned char pattern[], DWORD fileSize, SIZE_T numOfBytes){
	
	int i = 0;
	
	fileSize = fileSize - numOfBytes;
	
	if(fileSize > 0 )
	{
		for( i = 0 ; i <= fileSize; i++)
		{
			
			if(memcmp(pattern,fileBuff+i,numOfBytes) == 0)
				return i;
		}
	}

	return 0;
}


void banner(void){
	
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	COORD coordScreen = { 0, 0 };
	DWORD bytesWritten = 0;
	DWORD size = 0;

    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
	
	size = consoleInfo.dwSize.X * consoleInfo.dwSize.Y;

    SetConsoleTextAttribute(hConsole, 7);
	
	
	
	puts("                           888                   888      d8888   d8888  ");
	puts("                           888                   888        888     888  ");
	puts(".d8888b  888  888 88888b.  888  .d88b.   .d8888b 888  888   888     888  ");
	puts("88K      888  888 888  88b 888 d88  88b d88P     888 .88P   888     888  ");
	puts(" Y8888b. 888  888 888  888 888 888  888 888      888888K    888     888  ");
	puts("     X88 Y88b 888 888  888 888 Y88..88P Y88b.    888  88b   888     888  ");
	puts(" 88888P'   Y88888 888  888 888   Y88P     Y8888P 888  888 8888888 8888888");
	puts("\n\t\tCoded by Aleksandar 'revengsmK' (github.com/revengsmK)\n\n");
	
	FillConsoleOutputAttribute(hConsole,11,size,coordScreen,&bytesWritten);
	
	puts("Windows 11 Pro Personalization settings unlock tool");
	puts("===================================================================\n");  
	
}

int main(void){
	
	
	DWORD bytesWritten = 0;
	BOOL retVal;
	DWORD openErr = 0;
	HANDLE hFile = NULL;
	DWORD offset = 0;
	DWORD fileSize = 0;
	
	unsigned char pattern[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x4C, 0x24, 0x30}; // sub rsp, 28 | lea rcx,qword ptr ss:[rsp+30]
	unsigned char patch_bytes[] = { 0xB0, 0x01, 0xC3 }; // mov al,1 | ret
	unsigned char *fileBuf = NULL;
	char dllPath[MAX_PATH] = "";
	char bakPath[MAX_PATH] = "";
	
	
	banner();
	
	
	if(IsElevated() == FALSE){
		puts("[!] You don't have administrative privileges!\n\nRun this program as administrator.\n");
		getch();
		return -1;
	}
	
	if(setDebugPriv() == FALSE){
		puts("[!] Failed to set SeDebugPrivilege!\nAborting...\n");
		getch();
		return -2;
		
	}
	
	GetWindowsDirectory(dllPath,MAX_PATH);
	
	snprintf(dllPath + strlen(dllPath),MAX_PATH - strlen(dllPath),"%s","\\System32\\SettingsEnvironment.Desktop.dll");
	snprintf(bakPath,MAX_PATH,"%s%s",dllPath,".BAK");
	
	// Kill all existing SystemSettings.exe processes
	
	puts("-> Killing all existing SystemSettings.exe processes...\n");
	
	KillProcessByName("SystemSettings.exe");
	
	puts("-> Killing specific RuntimeBroker.exe process...\n");
	
	KillRuntimeBroker();
	
	puts("-> Waiting until all processes terminate...\n");
	Sleep(2000);
	
	puts("-> Making backup of original DLL (SettingsEnvironment.Desktop.dll) file...\n");
	
	CopyFile(dllPath,bakPath, TRUE);
	
	
	hFile = CreateFile(dllPath,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	
	openErr = GetLastError();
		 
	if(hFile == INVALID_HANDLE_VALUE)
	{
		puts("[!] Failed to open file!");
		
		if(openErr == ERROR_ACCESS_DENIED)
			puts("[!] Access denied!\n\nPlease run setpm.bat script before running sunlock11!");
	
		getch();
		return -3;
	}
	else
	{
		puts("    [+] DLL file successfully opened!");
		
		fileSize = GetFileSize(hFile,NULL);
	
		printf("    [+] DLL file size: %d bytes\n",(int)fileSize);
	
		if(fileSize != INVALID_FILE_SIZE)
		{
			// reserve memory block to load file
			fileBuf = GlobalAlloc(GPTR,fileSize);
		
			// load file content to the allocated memory block
			if(fileBuf == NULL)
			{
				puts("[!] Falied to allocate buffer!");
				goto cleanup;
			}
			
			ReadFile(hFile,fileBuf,fileSize,NULL,NULL);
		
			offset = seekbytes(fileBuf,pattern,fileSize,9);
		
			if(offset == 0)
			{
				puts("[!] Pattern not found!\nAborting...");
				goto cleanup;
			}
			
			printf("    [+] DLL file offset: 0x%X\n\n",(unsigned int)offset);
		}
		
		if(SetFilePointer(hFile, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER){
			 puts("[!] Invalid file pointer!");
			 goto cleanup;
		
		 }
		
		 // patch DLL
		 
		retVal = WriteFile(hFile,&patch_bytes,3,&bytesWritten,NULL);
		
		 if(bytesWritten != 3 || retVal == FALSE){
			  puts("[!] Failed to patch DLL file!");
			  goto cleanup;
			
		 }
		 
		puts("-> Successfully patched DLL file!");

	}

cleanup:
   GlobalFree(fileBuf);
   CloseHandle(hFile);  
	
	puts("\nPress any key to exit...");
	getch();

	return 0;
}