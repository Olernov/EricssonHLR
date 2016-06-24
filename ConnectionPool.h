#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Common.h"

using namespace std;
//using namespace boost::asio;
//using boost::asio::ip::tcp;

#define STR_TERMINATOR				'\0'
#define CR_CHAR_CODE				'\r'
#define LF_CHAR_CODE				'\n'

class ConnectionPool
{
public:
	ConnectionPool();
	~ConnectionPool();
	bool Initialize(const Config& config, string& errDescription);
	bool TryAcquire(unsigned int& index);
	int ExecCommand(unsigned int index, char* pTask, char* pResult);
	bool Close();
	
private:
	static const int receiveBufferSize = 65000;
	static const int sendBufferSize = 1024;
	int m_sockets[MAX_THREADS];
	vector<thread> m_threads;
	bool m_connected[MAX_THREADS];
	atomic_bool m_busy[MAX_THREADS];
	bool m_finished[MAX_THREADS];
	condition_variable m_condVars[MAX_THREADS];
	mutex m_mutexes[MAX_THREADS];
	string m_tasks[MAX_THREADS];
	int m_resultCodes[MAX_THREADS];
	string m_results[MAX_THREADS];
	Config m_config;
	bool m_stopFlag;

	enum {
		TELNET_SE = 240,	/* End of subnegotiation parameters. */
		TELNET_NOP = 241,	/* No operation. */
		TELNET_DM = 242,	/* (Data Mark) The data stream portion of a
			* Synch. This should always be accompanied
			* by a TCP Urgent notification. */
		TELNET_BRK = 243,	/* (Break) NVT character BRK. */
		TELNET_IP = 244,	/* (Interrupt Process) The function IP. */
		TELNET_AO = 245,	/* (Abort output) The function AO. */
		TELNET_AYT = 246,	/* (Are You There) The function AYT. */
		TELNET_EC = 247,	/* (Erase character) The function EC. */
		TELNET_EL = 248,	/* (Erase Line) The function EL. */
		TELNET_GA = 249,	/* (Go ahead) The GA signal. */
		TELNET_SB = 250,	/* Indicates that what follows is
			* subnegotiation of the indicated option. */
		TELNET_WILL = 251,	/* Indicates the desire to begin performing,
			* or confirmation that you are now performing,
			* the indicated option. */
		TELNET_WONT = 252,	/* Indicates the refusal to perform, or to
			* continue performing, the indicated option. */
		TELNET_DO = 253,	/* Indicates the request that the other party
			* perform, or confirmation that you are
			* expecting the other party to perform, the
			* indicated option. */
		TELNET_DONT = 254,	/* Indicates the demand that the other party
			* stop performing, or confirmation that you
			* are no longer expecting the other party
			* to perform, the indicated option. */
		TELNET_IAC = 255	/* Data Byte 255. */
	};
	
	void WorkerThread(unsigned int index);
	bool ConnectSocket(unsigned int index, string& errDescription);
	bool LoginToHLR(unsigned int index, string& errDescription);
	bool Reconnect(unsigned int index, string& errDescription);
	void TelnetParse(unsigned char* recvbuf, int* bytesRecv, int socketFD);
	void FinishWithNetworkError(string logMessage, unsigned int index);
	int ProcessHLRCommand(unsigned int index, string &errDescription);
	bool HLRIgnoredMessage(string message);
	bool HLRMessageToRetry(string message);
	string GetWinsockError();
};

