// EricssonHLR.cpp : Defines the entry point for the console application.
//

 //#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdarg.h>
#include <Winsock2.h>
#include "ConfigContainer.h"
#include "LogWriter.h"
#include "ConnectionPool.h"

#define MAX_DMS_RESPONSE_LEN				1024
#define ERROR_INIT_PARAMS					-1
#define INIT_FAIL							-2
#define OPERATION_SUCCESS					0


Config config;
LogWriter logWriter;
ConnectionPool connectionPool;

__declspec (dllexport) int __stdcall InitService(char* szInitParams, char* szResult)
{
	try {
		string errDescription;
		if (!config.ReadConfigString(szInitParams, errDescription)) {
			strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length());
			return ERROR_INIT_PARAMS;
		}
		logWriter.Initialize(config.m_logPath, errDescription);
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, "***** Starting Ericsson HLR driver *****"));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("Original init string: ") + string(szInitParams)));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("Parsed init params: ")));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Host: ") + config.m_hostName));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Port: ") + to_string(config.m_port)));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Username: ") + config.m_username));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Password: ") + config.m_password));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Domain: ") + config.m_domain));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Log path: ") + config.m_logPath));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Ignored HLR messages file: ") +
			config.m_ignoreMsgFilename));
		logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, string("   Debug mode: ") + 
			to_string(config.m_debugMode)));

		if(!connectionPool.Initialize(config, errDescription)) {
			logWriter.Write(LogMessage(time_t(), MAIN_THREAD_NUM, "Unable to set up connection to given host: " + 
				errDescription));
			logWriter.Stop();
			strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length());
			return ERROR_INIT_PARAMS;
		}
	}
	catch(LogWriterException& e) {
		strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, e.m_message.c_str(), e.m_message.length());
		connectionPool.Close();
		logWriter.Stop();
		return INIT_FAIL;
	}
	catch(std::exception& e)	{
		//const char* pmessage = "Exception caught while trying to initialize.";
		strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, e.what(), strlen(e.what()));
		connectionPool.Close();
		logWriter.Stop();
		return INIT_FAIL;
	}
	return OPERATION_SUCCESS;
}


__declspec (dllexport) int __stdcall ExecuteCommand(char **pParam, int nParamCount, char* szResult)
{
	return 0;
}


__declspec (dllexport) int __stdcall DeInitService(char* szResult)
{
	connectionPool.Close();
	logWriter.Stop();
	return OPERATION_SUCCESS;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL,  DWORD fdwReason,  LPVOID lpvReserved)
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

int main(int argc, char* argv[])
{
	char initResult[1024];
	int initRes = InitService(argv[1], initResult);
	DeInitService(initResult);

	char dummy;

	std::cin >> dummy;
	return OPERATION_SUCCESS;
}