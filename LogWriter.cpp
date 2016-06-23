#include <iostream>
#include <string>
#include "LogWriter.h"



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
					cout << timeBuf << "  |  "
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
	return true;
}