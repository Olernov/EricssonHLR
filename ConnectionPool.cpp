#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <chrono>
#include <thread>
#include <string>
//#include <set>
#include <Winsock2.h>
#include <windows.h>
//#innclude <boost/noncopyable.hpp>
//#include <boost/bind.hpp>

#include "ConfigContainer.h"
#include "ConnectionPool.h"
#include "LogWriter.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;


//using boost::asio::ip::tcp;
extern LogWriter logWriter;

ConnectionPool::ConnectionPool() :
	m_stopFlag(false)
{
	m_lastUsed.store(0);
}


ConnectionPool::~ConnectionPool()
{
}


bool ConnectionPool::Initialize(const Config& config, string& errDescription)
{
	m_config = config;
	WSADATA wsaData;

	if(WSAStartup(MAKEWORD(2,2), &wsaData)) {
		// не удалось инициализировать WinSock
		errDescription = "Error initializing Winsock: " + to_string(WSAGetLastError());
		return false;
	}

	for (unsigned int index = 0; index < config.m_numThreads; ++index) {
		m_sockets[index] = INVALID_SOCKET;
		if (!ConnectSocket(index, errDescription)) {
			return false;
		}
		m_connected[index] = true;
		logWriter.Write("Connected to HLR successfully.", index+1);
		
#ifndef DONT_LOGIN_TO_HLR		
			if (!LoginToHLR(index, errDescription)) {
				return false;
			}
#endif
		m_busy[index] = false;
		m_finished[index] = false;		
	}
	
	for (unsigned int i = 0; i < config.m_numThreads; ++i) {
		m_threads.push_back(thread(&ConnectionPool::WorkerThread, this, i));
	}
	return true;
}


bool ConnectionPool::ConnectSocket(unsigned int index, string& errDescription)
{
	struct sockaddr_in addr;
    m_sockets[index] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sockets[index] == INVALID_SOCKET) {
		errDescription = "Error creating socket. " + GetWinsockError();
		return false;
	}
	memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(m_config.m_hostName.c_str());
	if (addr.sin_addr.s_addr==INADDR_NONE) {
		errDescription = "Error parsing host address: " + m_config.m_hostName + GetWinsockError();
		return false;
	}
    addr.sin_port = htons(m_config.m_port);
	if(connect(m_sockets[index], (SOCKADDR*) &addr, sizeof(addr))==SOCKET_ERROR) {
		errDescription = "Unable to connect to host " + m_config.m_hostName + GetWinsockError();
		return false;
    }
	u_long iMode=1;
	if(ioctlsocket(m_sockets[index], FIONBIO, &iMode) != 0) {
		// Catch error
		errDescription = "Error setting socket in non-blocking mode. " + GetWinsockError();
		return false;
	}
	return true;
}


bool ConnectionPool::LoginToHLR(unsigned int index, string& errDescription)
{
	fd_set read_set;
    struct timeval tv;
	int bytesRecv = 0;
	char recvbuf[receiveBufferSize];
	char sendbuf[sendBufferSize]; 
	char response[receiveBufferSize];
	int nAttemptCounter = 0;

	while(nAttemptCounter < 10) {
		if(m_config.m_debugMode > 0) 
			logWriter.Write("Login attempt " + to_string(nAttemptCounter+1), index+1);
		
		response[0] = STR_TERMINATOR;

		while(true) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			FD_ZERO( &read_set );
			FD_SET( m_sockets[index], &read_set );
			if (select( m_sockets[index] + 1, &read_set, NULL, NULL, &tv ) != 0 ) {
				// check for message
				if (FD_ISSET( m_sockets[index], &read_set))  {
					// receive some data from server
					recvbuf[0] = STR_TERMINATOR;
					if ((bytesRecv = recv( m_sockets[index], recvbuf, sizeof(recvbuf), 0 ) ) == SOCKET_ERROR)  {
						errDescription = "LoginToHLR: Error receiving data from host" + GetWinsockError();
						return false;
					}
					else {
						TelnetParse((unsigned char*) recvbuf, &bytesRecv, m_sockets[index]);
						if (bytesRecv>0) {
							recvbuf[bytesRecv] = STR_TERMINATOR;
							if (m_config.m_debugMode > 0) 
								logWriter.Write("LoginToHLR: HLR response: " + string(recvbuf), index+1);
							_strupr_s(recvbuf, receiveBufferSize);
							if(strstr(recvbuf, "LOGIN:")) {
								// server asks for login
								if (m_config.m_debugMode > 0) 
									logWriter.Write("Sending username: " + m_config.m_username);
								sprintf_s((char*) sendbuf, sendBufferSize, "%s\r\n", m_config.m_username.c_str());
								if(send( m_sockets[index], sendbuf, strlen(sendbuf), 0 ) == SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}

							if(strstr(recvbuf,"PASSWORD:")) {
								// server asks for password
								if (m_config.m_debugMode > 0) 
									logWriter.Write("Sending password: " + m_config.m_password, index+1);
								sprintf_s((char*) sendbuf, sendBufferSize, "%s\r\n", m_config.m_password.c_str());
								if(send(m_sockets[index], sendbuf, strlen(sendbuf), 0) == SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}

							if(strstr(recvbuf,"DOMAIN:")) {
								// server asks for domain
								if (m_config.m_debugMode > 0) 
									logWriter.Write("Sending domain: " + m_config.m_domain, index+1);
								sprintf_s((char*)sendbuf, sendBufferSize, "%s\r\n", m_config.m_domain.c_str());
								if(send( m_sockets[index], sendbuf,strlen(sendbuf), 0 )==SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}

							if(strstr(recvbuf,"TERMINAL TYPE?")) {
								// server asks for domain
								if (m_config.m_debugMode > 0) 
									logWriter.Write("Sending terminal type: vt100", index+1);
								sprintf_s((char*)sendbuf, sendBufferSize, "vt100\r\n");
								if(send( m_sockets[index], sendbuf,strlen(sendbuf), 0 )==SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}
							if(strstr(recvbuf, HLR_PROMPT)) {
								return true;
							}
							if(strstr(recvbuf, "AUTHORIZATION FAILURE")) {
								errDescription = "LoginToHLR: authorization failure.";
								return false;
							}
							if(strstr(recvbuf, "SUCCESS")) {
								return true;
							}
							if(char* p=strstr(recvbuf,"ERR")) {
								// cut info from response
								size_t pos=strcspn(p,";\r\n");
								*(p + pos) = STR_TERMINATOR;
								errDescription =  "Unable to log in HLR: " + string(p);
								return false;
							}
						}
					}
				}
			}
			else {
				if (m_config.m_debugMode > 0) 
					logWriter.Write("LoginToHLR: select time-out", index+1);
				nAttemptCounter++;
				break;
			}
		}

	}
	errDescription = "Unable to login to Ericsson HLR.";
	return false;
}


bool ConnectionPool::Reconnect(unsigned int index, string& errDescription)
{
	logWriter.Write("Trying to reconnect ...", index);
	shutdown(m_sockets[index], SD_BOTH);
	closesocket(m_sockets[index]);
	m_connected[index] = false;
	if(ConnectSocket(index, errDescription)) {
		if(LoginToHLR(index, errDescription)) {
			logWriter.Write("Reconnected and logged in successfully", index);
			m_connected[index] = true;
			return true;
		}
	}
	
	logWriter.Write("Unable to reconnect.", index);
	return false;
}


// This code is taken from NetCat project http://netcat.sourceforge.net/
void ConnectionPool::TelnetParse(unsigned char* recvbuf,int* bytesRecv,int socketFD)
{
	/*static*/ unsigned char getrq[4];
	unsigned char putrq[4], *buf=recvbuf;
	int eat_chars=0;
	int l = 0;
	/* loop all chars of the string */
	for(int i=0; i<*bytesRecv; i++)
	{
		 if (recvbuf[i]==0) {
			// иногда в ответах HLR проскакивают нули, которые воспринимаются как концы строк. Заменим их на пробел
			recvbuf[i]=' ';
			continue;
		}

		/* if we found IAC char OR we are fetching a IAC code string process it */
		if ((recvbuf[i] != TELNET_IAC) && (l == 0))
			continue;
		/* this is surely a char that will be eaten */
		eat_chars++;
		/* copy the char in the IAC-code-building buffer */
		getrq[l++] = recvbuf[i];
		/* if this is the first char (IAC!) go straight to the next one */
		if (l == 1)
			continue;
		/* identify the IAC code. The effect is resolved here. If the char needs
   further data the subsection just needs to leave the index 'l' set. */
	switch (getrq[1]) {
		case TELNET_SE:
		case TELNET_NOP:
		  goto do_eat_chars;
		case TELNET_DM:
		case TELNET_BRK:
		case TELNET_IP:
		case TELNET_AO:
		case TELNET_AYT:
		case TELNET_EC:
		case TELNET_EL:
		case TELNET_GA:
		case TELNET_SB:
		  goto do_eat_chars;
		case TELNET_WILL:
		case TELNET_WONT:
		  if (l < 3) /* need more data */
			continue;

		  /* refuse this option */
		  putrq[0] = 0xFF;
		  putrq[1] = TELNET_DONT;
		  putrq[2] = getrq[2];
		  /* FIXME: the rfc seems not clean about what to do if the sending queue
			 is not empty.  Since it's the simplest solution, just override the
			 queue for now, but this must change in future. */
		  //write(ncsock->fd, putrq, 3);		
		  send( socketFD, (char*)putrq,3, 0 );
/* FIXME: handle failures */
		  goto do_eat_chars;
		case TELNET_DO:
		case TELNET_DONT:
		  if (l < 3) /* need more data */
			continue;

		  /* refuse this option */
		  putrq[0] = 0xFF;
		  putrq[1] = TELNET_WONT;
		  putrq[2] = getrq[2];
		  //write(ncsock->fd, putrq, 3);
		  send( socketFD, (char*)putrq,3, 0 );
		  goto do_eat_chars;
		case TELNET_IAC:
		  /* insert a byte 255 in the buffer.  Note that we don't know in which
			 position we are, but there must be at least 1 eaten char where we
			 can park our data byte.  This effect is senseless if using the old
			 telnet codes parsing policy. */
		  buf[i - --eat_chars] = 0xFF;
		  goto do_eat_chars;
		
		default:
		  /* FIXME: how to handle the unknown code? */
		  break;
		}
	continue;

do_eat_chars:
	/* ... */
	l = 0;
	if (eat_chars > 0) {
	  unsigned char *from, *to;

	  /* move the index to the overlapper character */
	  i++;

	  /* if this is the end of the string, memmove() does not care of a null
		 size, it simply does nothing. */
	  from = &buf[i];
	  to = &buf[i - eat_chars];
	  memmove(to, from, *bytesRecv - i);

	  /* fix the index.  since the loop will auto-increment the index we need
		 to put it one char before.  this means that it can become negative
		 but it isn't a big problem since it is signed. */
	  i -= eat_chars + 1;
	  *bytesRecv -= eat_chars;
	  eat_chars = 0;
	  }
	}
}

void ConnectionPool::FinishWithNetworkError(string logMessage, unsigned int index)
{
	logWriter.Write("ExecuteCommand: " + logMessage + GetWinsockError(), index);
	m_resultCodes[index] = TRY_LATER;
	m_results[index] = "Network error sending message to HLR";
	m_finished[index] = true;
	m_condVars[index].notify_one();
}


bool ConnectionPool::HLRIgnoredMessage(string message)
{
	for (auto it : m_config.m_ignoredHLRMessages) {
		if (message.find(it) != string::npos)
			return true;
	}
	return false;
}


bool ConnectionPool::HLRMessageToRetry(string message)
{
	for (auto it : m_config.m_retryHLRMessages) {
		if (message.find(it) != string::npos)
			return true;
	}
	return false;
}

int ConnectionPool::ProcessHLRCommand(unsigned int index, string& errDescription)
{
	if(!m_connected[index]) {
		logWriter.Write("Not connected to HLR", index);
		if (!Reconnect(index, errDescription)) {
			return TRY_LATER;
		}
	}

	char recvBuf[receiveBufferSize];
	char sendBuf[sendBufferSize]; 
	char hlrResponse[receiveBufferSize];
	int bytesRecv = recv(m_sockets[index], recvBuf, receiveBufferSize, 0) ;
	if (bytesRecv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)    {
		logWriter.Write("Error receiving data from host" + GetWinsockError(), index);
		if (!Reconnect(index, errDescription)) {
			return TRY_LATER;
		}
	}
	if(bytesRecv > 0) {
		TelnetParse((unsigned char*) recvBuf, &bytesRecv, m_sockets[index]);
		recvBuf[bytesRecv]='\0';
		logWriter.Write(string("HLR initiated response: ") + recvBuf, index);
		_strupr_s(recvBuf, receiveBufferSize);
		if(strstr(recvBuf, "TIME OUT") || strstr(recvBuf, "CONNECTION INTERRUPTED")) {
			logWriter.Write("TIME OUT or CONNECTION INTERRUPTED report from HLR, restoring connection ...", index);
			sprintf_s(sendBuf, sendBufferSize, "\r\n");
			if(send(m_sockets[index],(char*) sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR) {
				errDescription = "Socket error when sending restore connection message" + GetWinsockError();
				return TRY_LATER;
			}
		}
	}
	sprintf_s(sendBuf, sendBufferSize, "%s\r\n", m_tasks[index]);
	if(send(m_sockets[index], (char*) sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR) {
		errDescription = "Socket error when sending command" + GetWinsockError();
		return TRY_LATER;
	}
	fd_set read_set;
	struct timeval tv;
	hlrResponse[0] = STR_TERMINATOR;
	while (!m_finished[index]) {
		tv.tv_sec = SOCKET_TIMEOUT_SEC;
		tv.tv_usec = 0;
		FD_ZERO(&read_set);
		FD_SET(m_sockets[index], &read_set);
		if (select(m_sockets[index] + 1, &read_set, NULL, NULL, &tv) != 0) {
			// check for message
			if (FD_ISSET(m_sockets[index], &read_set)) {
				// receive some data from server
				int bytesRecv = recv(m_sockets[index], recvBuf, receiveBufferSize, 0);
				if (bytesRecv == SOCKET_ERROR)    {
					errDescription = "Socket error when receiving data" + GetWinsockError();
					return TRY_LATER;
				}
				else {
					TelnetParse((unsigned char*)recvBuf, &bytesRecv, m_sockets[index]);
					if (bytesRecv > 0) {
						recvBuf[bytesRecv] = STR_TERMINATOR;
						if (m_config.m_debugMode > 0)
							logWriter.Write(string("HLR response: ") + recvBuf, index);
						_strupr_s(recvBuf, receiveBufferSize);
						strncat_s(hlrResponse, receiveBufferSize, recvBuf, bytesRecv + 1);

						// check HLR ignored message list
						if (strstr(hlrResponse, "EXECUTED") || HLRIgnoredMessage(hlrResponse)) {
							if (m_config.m_debugMode > 0 && HLRIgnoredMessage(hlrResponse))
								logWriter.Write(string("HLR ignored message found"), index);
							errDescription = "Successful execution.";
							return OPERATION_SUCCESS;
						}
						
						if (char* p = strstr(hlrResponse, "NOT ACCEPTED")) {
							p += strlen("NOT ACCEPTED");
							StripHLRResponse(p, errDescription);
							return (HLRMessageToRetry(hlrResponse) ? TRY_LATER : CMD_NOTEXECUTED);
						}

						if (strstr(hlrResponse, m_tasks[index].c_str())
								&& !strcmp(hlrResponse + strlen(hlrResponse) - 2, HLR_PROMPT)) {
							// if HLR answers with echo and prompt then send ';'
							if (m_config.m_debugMode > 0) 
								logWriter.Write("Command echo received. Sending CRLF ...", index);
							const char* crlf = "\r\n";
							if (send(m_sockets[index], crlf, strlen(crlf), 0) == SOCKET_ERROR) {
								errDescription = "Socket error when sending data" + GetWinsockError();
								return TRY_LATER;
							}
							hlrResponse[0] = STR_TERMINATOR; // start composing new HLR response
						}
					}
					else {
						if (m_config.m_debugMode > 0)
							logWriter.Write("No bytes to read", index);
						if (strlen(hlrResponse)) {
							errDescription = string("Unable to parse HLR response:\n") + hlrResponse;
							return CMD_UNKNOWN;
						}
						else {
							errDescription = "No response received from HLR.";
							return TRY_LATER;
						}
					}
				}
			}
		}
		else {
			if (m_config.m_debugMode > 0)
				logWriter.Write("ExecuteCommand: socket time-out", index);
			if (strlen(hlrResponse) > 0) {
				errDescription = string("Unable to parse HLR response:\n") + hlrResponse;
				return CMD_UNKNOWN;
			}
			else {
				errDescription = "No response received from HLR.";
				return TRY_LATER;
			}
		}
	}
	return OPERATION_SUCCESS;
}

void ConnectionPool::StripHLRResponse(char* start, std::string& result)
{
	start += strspn(start, " ;\r\n");
	// remove spaces, \r and \n at the end of string
	char* responseEnd = start + strlen(start) - 1;
	char* end = responseEnd;
	while (end > start && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) {
		end--;
	}
	if ((end > start) && (end < responseEnd)) {
		*(end + 1) = STR_TERMINATOR;
	}

	// replace \r and \n with spaces 
	for (char* p3 = start; p3 < end; p3++) {
		if (*p3 == '\r' || *p3 == '\n') {
			*p3 = ' ';
		}
	}
	if (!strcmp(start + strlen(start) - 2, HLR_PROMPT)) {
		start[strlen(start) - 2] = STR_TERMINATOR;
	}
	result = start;
}


void ConnectionPool::WorkerThread(unsigned int index)
{
	while (!m_stopFlag) {
		unique_lock<mutex> locker(m_mutexes[index]);
		while(!m_stopFlag && !m_busy[index]) 
			m_condVars[index].wait(locker);
		if (!m_stopFlag && m_busy[index] && !m_finished[index]) {
			string errDescription;
			try {
				int res = ProcessHLRCommand(index, errDescription);
				logWriter.Write("Finished task: " + m_tasks[index], index);
				logWriter.Write("Task result: " + to_string(res), index);
				logWriter.Write("Description: " + errDescription, index);
				m_resultCodes[index] = res;
				m_results[index] = errDescription;
			}
			catch(const exception& ex) {
				logWriter.Write(string("Exception caught: ") + ex.what(), index);
				m_resultCodes[index] = EXCEPTION_CAUGHT;
				m_results[index] = ex.what();
			}
			catch(...) {
				logWriter.Write("Unknown exception caught", index);
				m_resultCodes[index] = EXCEPTION_CAUGHT;
				m_results[index] = "Unknown exception";
			}
			m_finished[index] = true;
			m_condVars[index].notify_one();
		}
	}
}


bool ConnectionPool::TryAcquire(unsigned int& index)
{
	const int maxSecondsToAcquire = 2;
	int cycleCounter = 0;

	time_t startTime;
	time(&startTime);
	bool firstCycle = true;
	while (true) {
		// Start looping from last used connection + 1 to ensure consequent connections using and 
		// to avoid suspending rarely used connections
		for (int i = (firstCycle ? ((m_lastUsed + 1) %  m_config.m_numThreads) : 0); i < m_config.m_numThreads; ++i) {
			bool oldValue = m_busy[i];
			if (!oldValue) {
				if (m_busy[i].compare_exchange_weak(oldValue, true)) {
					index = i;
					m_finished[i] = false;
					m_lastUsed.store(i);
					return true;
				}
			}
		}
		firstCycle = false;
		++cycleCounter;
		if (cycleCounter > 1000) {
			// check time elapsed
			time_t now;
			time(&now);
			if (now - startTime > maxSecondsToAcquire)
				return false;
			cycleCounter = 0;
		}
	}
}


int ConnectionPool::ExecCommand(unsigned int index, char* pTask, char* pResult)
{
	m_tasks[index] = pTask;
	m_condVars[index].notify_one();
	
	unique_lock<mutex> locker(m_mutexes[index]);
	while(!m_finished[index])
		m_condVars[index].wait(locker);
	int resultCode = m_resultCodes[index];
	if (m_config.m_debugMode > 0) {
		logWriter.Write("response length: " + to_string(m_results[index].length()), index);
	}
	strncpy_s(pResult, MAX_DMS_RESPONSE_LEN, m_results[index].c_str(), _TRUNCATE);
	if (m_config.m_debugMode > 0) {
		logWriter.Write("result length: " + to_string(strlen(pResult)), index);
	}
	m_busy[index] = false;
	return resultCode;
}


bool ConnectionPool::Close()
{
	for(auto& socket : m_sockets) {
		shutdown(socket, SD_BOTH);
		closesocket(socket);
	}
	m_stopFlag = true;
	for (int i = 0; i < m_config.m_numThreads; ++i) {
		m_condVars[i].notify_one();
	}
	for(auto& thr : m_threads) {
		if (thr.joinable())
			thr.join();
	}
	WSACleanup();
	return true;
}

string ConnectionPool::GetWinsockError()
{
	//WCHAR* errString = NULL;
	//int size = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
 //                FORMAT_MESSAGE_FROM_SYSTEM, // use windows internal message table
 //                0,       // 0 since source is internal message table
 //                WSAGetLastError(), // this is the error code returned by WSAGetLastError()
 //                         // Could just as well have been an error code from generic
 //                         // Windows errors from GetLastError()
 //                0,       // auto-determine language to use
 //                errString, // this is WHERE we want FormatMessage
 //                                   // to plunk the error string.  Note the
 //                                   // peculiar pass format:  Even though
 //                                   // errString is already a pointer, we
 //                                   // pass &errString (which is really type LPSTR* now)
 //                                   // and then CAST IT to (LPSTR).  This is a really weird
 //                                   // trip up.. but its how they do it on msdn:
 //                                   // http://msdn.microsoft.com/en-us/library/ms679351(VS.85).aspx
 //                0,                 // min size for buffer
 //                0);               // 0, since getting message from system tables
	//
 //   LocalFree( errString ) ; 

	return ". Error code: " + to_string(WSAGetLastError()) + ". " ;
}
