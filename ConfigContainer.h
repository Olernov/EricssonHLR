#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <map>

using namespace std;


class Config
{
public:
	Config() {};
	bool ReadConfigString(const string& configString, string& errDescription);
	
	string m_hostName;
	unsigned long m_port;
	string m_username;
	string m_password;
	string m_domain;
	string m_logPath;
	string m_ignoreMsgFilename;
	unsigned long m_debugMode;
	unsigned long  m_numThreads;
};