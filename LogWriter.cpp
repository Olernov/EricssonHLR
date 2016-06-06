#include <iostream>
#include "LogWriter.h"



void LogWriter::WriteThreadFunction()
{
	while (true) {
		while (!messageQueue.empty()) {
			LogMessage* pmessage; 
			messageQueue.pop(pmessage);
			cout << pmessage->m_message.c_str() << endl;
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
	LogMessage* pnewMessage = new LogMessage(time_t(), threadIndex, message);
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