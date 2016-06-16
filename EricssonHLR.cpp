// EricssonHLR.cpp : Defines the entry point for the console application.
//

 //#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdarg.h>
#include <Winsock2.h>
#include "Common.h"
#include "ConfigContainer.h"
#include "LogWriter.h"
#include "ConnectionPool.h"

Config config;
LogWriter logWriter;
ConnectionPool connectionPool;
mutex g_coutMutex;

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


__declspec (dllexport) int __stdcall ExecuteCommand(char **pParam, int nParamCount, char* pResult)
{
	try {
		char* pCommand = pParam[0];
		logWriter << "-----**********************----";
		logWriter << string("ExecuteCommand: ") + pCommand;
		if (strlen(pCommand) > MAX_COMMAND_LEN) {
			strcpy_s(pResult, MAX_DMS_RESPONSE_LEN, "Command is too long.");
			logWriter << "Error: Command length exceeds 1020 symbols. Won't send to HLR.";
			return ERROR_CMD_TOO_LONG;
		}
		// check logwriter exception
		if (logWriter.GetException() != nullptr) {
			try {
				rethrow_exception(logWriter.GetException());
			}
			catch (const exception& ex) {
				strncpy_s(pResult, MAX_DMS_RESPONSE_LEN, ex.what(), strlen(ex.what()) + 1);
				logWriter << "LogWriter exception detected. Command won't execute until fixed";
				logWriter.ClearException();
				return EXCEPTION_CAUGHT;
			}
		}

		unsigned int connIndex;
		logWriter.Write(string("Trying to acquire connection for: ") + pCommand);
		if (!connectionPool.TryAcquire(connIndex)) {
			logWriter << string("No free connection for: ") + pCommand;
			logWriter << "Sending TRY_LATER result";
			return TRY_LATER;
		}
		logWriter.Write("Acquired connection #" + to_string(connIndex) + " for: " + pCommand);
		return connectionPool.ExecCommand(connIndex, pParam[0], pResult);
	}
	catch(const exception& ex) {
		strncpy_s(pResult, MAX_DMS_RESPONSE_LEN, ex.what(), strlen(ex.what()) + 1);
		return EXCEPTION_CAUGHT;
	}
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

void TestCommandSender(int index)
{
	srand((unsigned int)time(NULL));
	logWriter.Write(string("Started test command sender thread #") + to_string(index));
	for (int i = 0; i < 10; ++i) {
		char* task = new char[50];
		char* result = new char[MAX_DMS_RESPONSE_LEN];
		sprintf_s(task, 50, "EXECUTE HLR COMMAND %d", index * 10 + i);
		result[0] = '\0';
		int res = ExecuteCommand( &task, NUM_OF_EXECUTE_COMMAND_PARAMS, result);
		
		{
			lock_guard<mutex> lock(g_coutMutex);
			cout << "result code: " << res << endl;
			cout << "result message: " << result << endl;
		}
		this_thread::sleep_for(std::chrono::seconds(1 + rand() % 3));
		delete task;
		delete result;
	}
}

int main(int argc, char* argv[])
{
	// This is a test entry-point, because our target is DLL and main() is not exported.
	// Different tests may be implemented here. Set configuration type to Application (*.exe)
	// and run tests. If successful, set config to DLL and deploy it to DMS.

	char initResult[1024];
	int initRes = InitService(argv[1], initResult);
	if (initRes == OPERATION_SUCCESS) {
		vector<thread> cmdSenderThreads;
		for (int i = 0; i < 5; i++) {
			cmdSenderThreads.push_back(thread(TestCommandSender, i));
			this_thread::sleep_for(std::chrono::seconds(rand() % 2));
		}
		for(auto& thr : cmdSenderThreads) {
			thr.join();
		}
	}
	DeInitService(initResult);

	char dummy;
	cout << ">";
	std::cin >> dummy;
	return OPERATION_SUCCESS;
}