#include "ConfigContainer.h"
#include "Common.h"

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
					m_ignoredMsgFilename = option_value;
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
	else if (m_numThreads > MAX_THREADS)
		errDescription = "Invalid parameter " + to_string(m_numThreads) + 
			" passed for NUM_THREADS. Valid values from 1 to 8.";

	return errDescription.empty();
}


bool Config::ReadIgnoredMsgFile(string& errDescription)
{
	ifstream ignoredMsgFile(m_ignoredMsgFilename);
	if (!ignoredMsgFile.is_open()) {
		errDescription = "Could not open ignored msg file: " + m_ignoredMsgFilename;
		return false;
	}
	enum {
		nothing,
		ignore_section,
		retry_section
	} state = nothing;
	string line;
	bool result = false;
	errDescription.clear();
	try {
		while (getline(ignoredMsgFile, line)) {
			size_t pos;
			if ((pos = line.find_first_not_of(" \t")) != string::npos)
				line = line.substr(pos);
			line.erase(line.find_last_not_of(" \t") + 1);
			if (!line.empty() ) {
				transform(line.begin(), line.end(), line.begin(), ::toupper);	
				switch (state) {
				case nothing:
					if (line == "[IGNORE]") {
						state = ignore_section;
					}
					else if (line == "[RETRY]") {
						throw runtime_error("ignored messages file: [RETRY] section found when [IGNORE] expected");
					}
					else {
						throw runtime_error("ignored messages file: expected [IGNORE] but found " + line);
					}
					break;
				case ignore_section:
					if (line == "[RETRY]") {
						state = retry_section;
					}
					else {
						m_ignoredHLRMessages.insert(line);
					}
					break;
				case retry_section:
					m_retryHLRMessages.insert(line);
					break;
				}
			}
		}
		// final state analyzis
		switch (state) {
		case nothing:
			throw runtime_error("[IGNORE] section not found in ignored messages file");
		case ignore_section:
			throw runtime_error("[RETRY] section not found in ignored messages file");
		case retry_section:
			result = true;
		}
	}
	catch(const exception& ex) {
		errDescription = ex.what();
	}
	ignoredMsgFile.close();
	return result;
}