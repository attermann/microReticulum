#pragma once

#ifndef NATIVE
#include <Arduino.h>
#endif

#include <string>

namespace RNS {

	enum LogLevel {
		LOG_CRITICAL = 0,
		LOG_ERROR    = 1,
		LOG_WARNING  = 2,
		LOG_NOTICE   = 3,
		LOG_INFO     = 4,
		LOG_VERBOSE  = 5,
		LOG_DEBUG    = 6,
		LOG_EXTREME  = 7,
	};

	void loglevel(LogLevel level);
	LogLevel loglevel();

	void doLog(const char *msg, LogLevel level);

	inline void log(const char *msg, LogLevel level = LOG_NOTICE) { doLog(msg, level); }
#ifndef NATIVE
	inline void log(const String msg, LogLevel level = LOG_NOTICE) { doLog(msg.c_str(), level); }
#endif
	inline void log(const std::string& msg, LogLevel level = LOG_NOTICE) { doLog(msg.c_str(), level); }

	inline void critical(const char *msg) { doLog(msg, LOG_CRITICAL); }
	inline void critical(const std::string& msg) { doLog(msg.c_str(), LOG_CRITICAL); }

	inline void error(const char *msg) { doLog(msg, LOG_ERROR); }
	inline void error(const std::string& msg) { doLog(msg.c_str(), LOG_ERROR); }

	inline void warning(const char *msg) { doLog(msg, LOG_WARNING); }
	inline void warning(const std::string& msg) { doLog(msg.c_str(), LOG_WARNING); }

	inline void notice(const char *msg) { doLog(msg, LOG_NOTICE); }
	inline void notice(const std::string& msg) { doLog(msg.c_str(), LOG_NOTICE); }

	inline void info(const char *msg) { doLog(msg, LOG_INFO); }
	inline void info(const std::string& msg) { doLog(msg.c_str(), LOG_INFO); }

	inline void verbose(const char *msg) { doLog(msg, LOG_VERBOSE); }
	inline void verbose(const std::string& msg) { doLog(msg.c_str(), LOG_VERBOSE); }

	inline void debug(const char *msg) { doLog(msg, LOG_DEBUG); }
	inline void debug(const std::string& msg) { doLog(msg.c_str(), LOG_DEBUG); }

	inline void extreme(const char *msg) { doLog(msg, LOG_EXTREME); }
	inline void extreme(const std::string& msg) { doLog(msg.c_str(), LOG_EXTREME); }

	void head(const char *msg, LogLevel level = LOG_NOTICE);
	inline void head(const std::string& msg, LogLevel level = LOG_NOTICE) { head(msg.c_str(), level); }

}
