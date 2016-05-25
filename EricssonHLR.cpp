// EricssonHLR.cpp : Defines the entry point for the console application.
//

 //#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <Winsock2.h>
#include "ConfigContainer.h"

#define MAX_DMS_RESPONSE_LEN				1024
#define ERROR_INIT_PARAMS					-1
#define INIT_SERVICE_SUCCESS				0

Config config;

__declspec (dllexport) int __stdcall InitService(char* szInitParams, char* szResult)
{
	string errDescription;
	if (!config.ReadConfigString(szInitParams, errDescription)) {
		strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length());
		return ERROR_INIT_PARAMS;
	}
	return INIT_SERVICE_SUCCESS;
}

__declspec (dllexport) int __stdcall ExecuteCommand(char **pParam, int nParamCount, char* szResult)
{
	return 0;
}

__declspec (dllexport) int __stdcall DeInitService(char* szResult)
{
	
	return 0;
}

BOOL WINAPI DllMain(  HINSTANCE hinstDLL,  DWORD fdwReason,  LPVOID lpvReserved)
{
	switch ( fdwReason )
	{
		case DLL_PROCESS_ATTACH:
			//InitializeCriticalSection(&csSocket);
			//InitializeCriticalSection(&csLogFile);
			break;
		case DLL_THREAD_ATTACH:
			// A process is creating a new thread.
		break;
		case DLL_THREAD_DETACH:
			// A thread exits normally.
			break;
		case DLL_PROCESS_DETACH:
			//DeleteCriticalSection(&csSocket);
			//DeleteCriticalSection(&csLogFile);
		break;
	}
	return TRUE;
}

int main(int, char*)
{
	char initResult[1024];
	int initRes = InitService("host =;username=1; port=555; logpath=; num_threads=123", initResult);
	return 0;
}