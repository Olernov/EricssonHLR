#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <chrono>
#include <thread>
#include <string>
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

ConnectionPool::ConnectionPool()
{
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
		m_sockets.push_back(INVALID_SOCKET);
		if (!ConnectSocket(index, errDescription)) {
			return false;
		}
		logWriter.Write("Connected to HLR successfully.", index);
		if (!LoginToHLR(index, errDescription)) {
			return false;
		}
		//logWriter.Write("LoginToHLR: logged in successfully.", index);
	}
	/*for (unsigned int i = 0; i < config.m_numThreads; ++i) {
		m_threads.push_back(thread(&ConnectionPool::WorkerThread, this, i));
	}*/
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
	fd_set read_set, error_set;
    struct timeval tv;
	int bytesRecv = 0;
	char recvbuf[receiveBufferSize];
	char sendbuf[sendBufferSize]; 
	char response[receiveBufferSize];
	int nAttemptCounter = 0;

	while(nAttemptCounter < 10) {
		if(m_config.m_debugMode > 0) 
			logWriter.Write("Login attempt " + to_string(nAttemptCounter+1), index);
		
		response[0] = STR_TERMINATOR;

		while(true) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			FD_ZERO( &read_set );
			FD_ZERO( &error_set );
			FD_SET( m_sockets[index], &read_set );
			FD_SET( m_sockets[index], &error_set );
			if (select( m_sockets[index] + 1, &read_set, NULL, &error_set, &tv ) != 0 ) {
				if (FD_ISSET(m_sockets[index], &error_set )) {
					errDescription = "LoginToHLR: Error on socket" + GetWinsockError();
					return false;
				}
				// check for message
				if (FD_ISSET( m_sockets[index], &read_set))  {
					// receive some data from server
					recvbuf[0] = STR_TERMINATOR;
					if ( ( bytesRecv = recv( m_sockets[index], recvbuf, sizeof(recvbuf), 0 ) ) == SOCKET_ERROR )  {
						errDescription = "LoginToHLR: Error receiving data from host" + GetWinsockError();
						return false;
					}
					else {
						TelnetParse((unsigned char*) recvbuf, &bytesRecv, m_sockets[index]);
						if (bytesRecv>0) {
							recvbuf[bytesRecv] = STR_TERMINATOR;
							if (m_config.m_debugMode > 0) 
								logWriter.Write("LoginToHLR: HLR response: " + string(recvbuf), index);
							_strupr_s(recvbuf, receiveBufferSize);
							if(strstr(recvbuf, "USERCODE:")) {
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
									logWriter.Write("Sending password: " + m_config.m_password);
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
									logWriter.Write("Sending domain: " + m_config.m_domain);
								sprintf_s((char*)sendbuf, sendBufferSize, "%s\r\n", m_config.m_domain.c_str());
								if(send( m_sockets[index], sendbuf,strlen(sendbuf), 0 )==SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}
							if(strstr(recvbuf,"\x03<")) {
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
					logWriter.Write("LoginToHLR: select time-out", index);
				nAttemptCounter++;
				break;
			}
		}

	}
	errDescription = "Unable to login to Ericsson HLR.";
	return false;
}


// This code is taken from NetCat project http://netcat.sourceforge.net/
void ConnectionPool::TelnetParse(unsigned char* recvbuf,int* bytesRecv,int socketFD)
{
	static unsigned char getrq[4];
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


void ConnectionPool::WorkerThread(unsigned int index)
{
	//logWriter.Write(LogMessage(time_t(), index, "Connection thread started"));
	//ip::tcp::socket socket(m_IO_service);
	//try {
	//	ip::tcp::resolver resolver(m_IO_service);
	//	ip::tcp::resolver::query query(m_config.m_hostName, to_string(m_config.m_port));
	//	ip::tcp::resolver::iterator iter = resolver.resolve(query);
	//	ip::tcp::endpoint ep = *iter;
	//	socket.connect(ep);
	//}
	//catch(const boost::system::system_error& e) {
	//	string errDescription = e.code().message();
	//	//	return false;
	//}
	//this_thread::sleep_for(chrono::seconds(3));
	//socket.close();
}


bool ConnectionPool::Close()
{
	for(auto& socket : m_sockets) {
		shutdown(socket, SD_BOTH);
		closesocket(socket);
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