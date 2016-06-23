#include <iostream>
#include <string>
#include <windows.h>
#include "LogWriter.h"


void LogWriter::CheckLogFile(time_t messageTime)
{
	tm messageTimeTm;
	localtime_s(&messageTimeTm, &messageTime);
	char dateBuf[30];
	sprintf_s(dateBuf, 30, "%4.4d%2.2d%2.2d", messageTimeTm.tm_year+1900, messageTimeTm.tm_mon+1, messageTimeTm.tm_mday);
	if(string(dateBuf) != m_logFileDate) {
		if(m_logStream.is_open()) {
			m_logStream.close();
		}
		char logName[MAX_PATH];
		if(!m_logPath.empty())
			sprintf_s(logName, MAX_PATH, "%s\\EricssonHLR_%s.log", m_logPath.c_str(), dateBuf);
		else
			sprintf_s(logName, MAX_PATH, "EricssonHLR_%s.log", dateBuf);
		m_logStream.open(logName, fstream::app|fstream::out);
		if (!m_logStream.is_open())
			throw runtime_error(string("Unable to open log file ") + string(logName));
		m_logFileDate = dateBuf;
	}
}


void LogWriter::WriteThreadFunction()
{
	while (true) {
		try {
			while (!messageQueue.empty()) {
				LogMessage* pMessage;
				if (messageQueue.pop(pMessage)) {
					tm messageTime;
					localtime_s(&messageTime, &pMessage->m_time);
					char timeBuf[30];
					strftime(timeBuf, 29, "%H:%M:%S", &messageTime);
					CheckLogFile(pMessage->m_time);
					m_logStream << timeBuf << "  |  "
						<< (pMessage->m_threadNum != MAIN_THREAD_NUM ? to_string(pMessage->m_threadNum) : " ")
						<< "  |  " << pMessage->m_message.c_str() << endl;
					delete pMessage;
				}
			}
			if (m_stopFlag && messageQueue.empty())
				return;
			if (messageQueue.empty())
				this_thread::sleep_for(chrono::seconds(sleepWhenQueueEmpty));
		}
		catch (...) {
			lock_guard<mutex> lock(m_exceptionMutex);
			if (m_excPointer == nullptr)
				// if exception is not set or previous exception is cleared then set new
				m_excPointer = current_exception();
		}
	}
}

LogWriter::LogWriter() 
	: messageQueue(queueSize), 
	m_stopFlag(false),
	m_excPointer(nullptr)
{
}


LogWriter::~LogWriter() 
{
}

bool LogWriter::Initialize(const string& logPath, string& errDescription)
{
	m_logPath = logPath;
	time_t now;
	time(&now);
	CheckLogFile(now);
	m_writeThread = thread(&LogWriter::WriteThreadFunction, this);
	return true;
}

bool LogWriter::Write(const LogMessage& message)
{
	try {
		LogMessage* pnewMessage = new LogMessage(message);
		if (!messageQueue.push(pnewMessage))
			throw LogWriterException("Unable to add message to log queue");
	}
	catch(...){
		cerr << "exception when LogWriter::Write" << endl;
	}
	return true;
}

bool LogWriter::Write(string message, short threadIndex)
{
	time_t now;
	time(&now);
	LogMessage* pnewMessage = new LogMessage(now, threadIndex, message);
	Write(*pnewMessage);
	return true;
}

void LogWriter::operator<<(const string& message)
{
	Write(message, MAIN_THREAD_NUM);
}

void LogWriter::ClearException()
{
	lock_guard<mutex> lock(m_exceptionMutex);
	m_excPointer = nullptr;
}


bool LogWriter::Stop()
{
	m_stopFlag = true;
	if (m_writeThread.joinable())
		m_writeThread.join();
	m_logStream.close();
	return true;
}