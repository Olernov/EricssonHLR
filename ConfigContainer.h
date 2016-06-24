#pragma once
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <set>

using namespace std;


class Config
{
public:
	Config() {};
	bool ReadConfigString(const string& configString, string& errDescription);
	bool ReadIgnoredMsgFile(string& errDescription);

	string m_hostName;
	unsigned long m_port;
	string m_username;
	string m_password;
	string m_domain;
	string m_logPath;
	string m_ignoredMsgFilename;
	unsigned long m_debugMode;
	unsigned long  m_numThreads;
	set<string> m_ignoredHLRMessages;
	set<string> m_retryHLRMessages;
};