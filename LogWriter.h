#pragma once
#include <queue>
#include <thread>
#include <time.h>
#include <boost/lockfree/queue.hpp>
#include "Common.h"

using namespace std;

class LogWriterException 
{
public:
	LogWriterException (const string& message) 
		: m_message(message) 
		{}
	string m_message;
};

class LogMessage
{
public:
	LogMessage();
	LogMessage(time_t time, short threadNum, string message)
		: m_time(time), m_threadNum(threadNum), m_message(message)
		{}
	time_t m_time;
	short m_threadNum;
	string m_message;
};

class LogWriter
{
public:
	LogWriter();
	~LogWriter();
	bool Initialize(const string& logPath, string& errDescription);
	bool Write(string message, short threadIndex = MAIN_THREAD_NUM);
	bool Write(const LogMessage&);
	
	bool Stop();
private:
	static const int queueSize = 128;
	static const int sleepWhenQueueEmpty = 1;
	string m_logPath;
	boost::lockfree::queue<LogMessage*> messageQueue;
	atomic<bool> m_stopFlag;
	thread m_writeThread;
	void WriteThreadFunction();
};