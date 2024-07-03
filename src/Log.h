#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <string>

#define LOG(msg, level) (RNS::log(msg, level))
#define HEAD(msg, level) (RNS::head(msg, level))
#define CRITICAL(msg) (RNS::critical(msg))
#define ERROR(msg) (RNS::error(msg))
#define WARNING(msg) (RNS::warning(msg))
#define NOTICE(msg) (RNS::notice(msg))
#define INFO(msg) (RNS::info(msg))
#define VERBOSE(msg) (RNS::verbose(msg))
#ifndef NDEBUG
	#define DEBUG(msg) (RNS::debug(msg))
	#define TRACE(msg) (RNS::trace(msg))
	//#define MEM(msg) (RNS::mem(msg))
	#define MEM(ignore) ((void)0)
#else
	#define DEBUG(ignore) ((void)0)
	#define TRACE(ignore) ((void)0)
	#define MEM(ignore) ((void)0)
#endif

namespace RNS {

	enum LogLevel {
		LOG_NONE     = 0,
		LOG_CRITICAL = 1,
		LOG_ERROR    = 2,
		LOG_WARNING  = 3,
		LOG_NOTICE   = 4,
		LOG_INFO     = 5,
		LOG_VERBOSE  = 6,
		LOG_DEBUG    = 7,
		LOG_TRACE    = 8,
		LOG_MEM      = 9
	};

	using log_callback = void(*)(const char* msg, LogLevel level);

	const char* getLevelName(LogLevel level);
	const char* getTimeString();

	void loglevel(LogLevel level);
	LogLevel loglevel();

	void setLogCallback(log_callback on_log = nullptr);

	void doLog(const char* msg, LogLevel level);
	void doLog(LogLevel level, const char* msg, ...);

	inline void log(const char* msg, LogLevel level = LOG_NOTICE) { doLog(msg, level); }
#ifdef ARDUINO
	inline void log(const String& msg, LogLevel level = LOG_NOTICE) { doLog(msg.c_str(), level); }
	inline void log(const StringSumHelper& msg, LogLevel level = LOG_NOTICE) { doLog(msg.c_str(), level); }
#endif
	inline void log(const std::string& msg, LogLevel level = LOG_NOTICE) { doLog(msg.c_str(), level); }

	void head(const char* msg, LogLevel level = LOG_NOTICE);
	inline void head(const std::string& msg, LogLevel level = LOG_NOTICE) { head(msg.c_str(), level); }

	inline void critical(const char* msg) { doLog(msg, LOG_CRITICAL); }
	inline void critical(const std::string& msg) { doLog(msg.c_str(), LOG_CRITICAL); }

	inline void error(const char* msg) { doLog(msg, LOG_ERROR); }
	inline void error(const std::string& msg) { doLog(msg.c_str(), LOG_ERROR); }

	inline void warning(const char* msg) { doLog(msg, LOG_WARNING); }
	inline void warning(const std::string& msg) { doLog(msg.c_str(), LOG_WARNING); }

	inline void notice(const char* msg) { doLog(msg, LOG_NOTICE); }
	inline void notice(const std::string& msg) { doLog(msg.c_str(), LOG_NOTICE); }

	inline void info(const char* msg) { doLog(msg, LOG_INFO); }
	inline void info(const std::string& msg) { doLog(msg.c_str(), LOG_INFO); }

	inline void verbose(const char* msg) { doLog(msg, LOG_VERBOSE); }
	inline void verbose(const std::string& msg) { doLog(msg.c_str(), LOG_VERBOSE); }

	inline void debug(const char* msg) { doLog(msg, LOG_DEBUG); }
	inline void debug(const std::string& msg) { doLog(msg.c_str(), LOG_DEBUG); }

	inline void trace(const char* msg) { doLog(msg, LOG_TRACE); }
	inline void trace(const std::string& msg) { doLog(msg.c_str(), LOG_TRACE); }

	inline void mem(const char* msg) { doLog(msg, LOG_MEM); }
	inline void mem(const std::string& msg) { doLog(msg.c_str(), LOG_MEM); }

}
