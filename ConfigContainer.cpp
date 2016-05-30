#include "ConfigContainer.h"

bool Config::ReadConfigString(const string& configString, string& errDescription)
{
	// read config string and parse params
	size_t pos = 0;
	m_numThreads = 0;
	while (pos < configString.length())
	{
		pos = configString.find_first_not_of(" \t;", pos);
		if (pos == string::npos)
			break;
		size_t delim_pos = configString.find_first_of(" \t=;", pos);
		string option_name;
		if (delim_pos != string::npos) {
			option_name = configString.substr(pos, delim_pos - pos);
			transform(option_name.begin(), option_name.end(), option_name.begin(), ::toupper);
			size_t value_pos = configString.find_first_not_of(" \t=", delim_pos);
			string option_value;  
			if (value_pos != string::npos) {
				size_t value_end = configString.find_first_of(" \t;", value_pos);
				if (value_end != string::npos) {
					option_value = configString.substr(value_pos, value_end - value_pos);
					pos = value_end + 1;
				}
				else {
					option_value = configString.substr(value_pos);
					pos = string::npos;
				}
				
				if (option_name.compare("HOST") == 0)
					m_hostName = option_value;
				else if (option_name.compare("PORT") == 0) {
					try {
						m_port = stoul(option_value);
					}
					catch (const std::invalid_argument&) {
						errDescription = "Invalid parameter " + option_value + " passed for PORT.";
						return false;
					}
				}
				else if (option_name.compare("USERNAME") == 0)
					m_username = option_value;
				else if (option_name.compare("PASSWORD") == 0)
					m_password = option_value;
				else if (option_name.compare("DOMAIN") == 0)
					m_domain = option_value;
				else if (option_name.compare("LOGPATH") == 0) {
					m_logPath = option_value;
				}
				else if (option_name.compare("IGNORE_MSG_FILE") == 0)
					m_ignoreMsgFilename = option_value;
				else if (option_name.compare("DEBUG") == 0) {
					try {
						m_debugMode = stoul(option_value);
					}
					catch (const std::invalid_argument&) {
						errDescription = "Invalid parameter " + option_value + " passed for DEBUG.";
						return false;
					}
				}
				else if (option_name.compare("NUM_THREADS") == 0) {
					try {
						m_numThreads = stoul(option_value);
					}
					catch (const std::invalid_argument&) {
						errDescription = "Invalid parameter " + option_value + " passed for NUM_THREADS.";
						return false;
					}
				}
			}
			else {
				pos = delim_pos + 1;
			}
		}
	}	

	// check mandatory params
	if (m_hostName.empty())
		errDescription = "Mandatory param HOST is missing.";
	else if (m_username.empty())
		errDescription = "Mandatory param USERNAME is missing.";
	else if (m_password.empty())
		errDescription = "Mandatory param PASSWORD is missing.";
	
	if (m_numThreads == 0) // not initialized
		m_numThreads = 1;
	else if (m_numThreads > 8)
		errDescription = "Invalid parameter " + to_string(m_numThreads) + 
			" passed for NUM_THREADS. Valid values from 1 to 8.";

	return errDescription.empty();
}
