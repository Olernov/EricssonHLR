#include <iostream>
#include "LogWriter.h"



void LogWriter::WriteThreadFunction()
{
	while (true) {
		while (!messageQueue.empty()) {
			LogMessage* pmessage; 
			messageQueue.pop(pmessage);
			tm messageTime;
			localtime_s(&messageTime, &pmessage->m_time);
			char timeBuf[30];
		    strftime(timeBuf, 29 , "%H:%M:%S", &messageTime);
			cout << timeBuf << "  |  " << pmessage->m_threadNum << "  |  " << pmessage->m_message.c_str() << endl;
			delete pmessage;
		}
		if (m_stopFlag && messageQueue.empty())
			return;
		if (messageQueue.empty())
			this_thread::sleep_for(chrono::seconds(sleepWhenQueueEmpty));
	}
}

LogWriter::LogWriter() 
	: messageQueue(queueSize), m_stopFlag(false)
{
}


LogWriter::~LogWriter() 
{
}

bool LogWriter::Initialize(const string& logPath, string& errDescription)
{
	m_logPath = logPath;
	m_writeThread = thread(&LogWriter::WriteThreadFunction, this);
	return true;
}

bool LogWriter::Write(const LogMessage& message)
{
	LogMessage* pnewMessage = new LogMessage(message);
	if (!messageQueue.push(pnewMessage))
		throw LogWriterException("Unable to add message to log queue");
	return true;
}

bool LogWriter::Write(string message, short threadIndex)
{
	time_t now;
	time(&now);
	LogMessage* pnewMessage = new LogMessage(now, threadIndex, message);
	if (!messageQueue.push(pnewMessage))
		throw LogWriterException("Unable to add message to log queue");
	return true;
}

bool LogWriter::Stop()
{
	m_stopFlag = true;
	if (m_writeThread.joinable())
		m_writeThread.join();
	return true;
}