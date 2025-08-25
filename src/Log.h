#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <string>

#define LOG(msg, level) (RNS::log(msg, level))
#define LOGF(level, msg, ...) (RNS::logf(level, msg, __VA_ARGS__))
#define HEAD(msg, level) (RNS::head(msg, level))
#define HEADF(level, msg, ...) (RNS::headf(level, msg, __VA_ARGS__))
#define CRITICAL(msg) (RNS::critical(msg))
#define CRITICALF(msg, ...) (RNS::criticalf(msg, __VA_ARGS__))
#define ERROR(msg) (RNS::error(msg))
#define ERRORF(msg, ...) (RNS::errorf(msg, __VA_ARGS__))
#define WARNING(msg) (RNS::warning(msg))
#define WARNINGF(msg, ...) (RNS::warningf(msg, __VA_ARGS__))
#define NOTICE(msg) (RNS::notice(msg))
#define NOTICEF(msg, ...) (RNS::noticef(msg, __VA_ARGS__))
#define INFO(msg) (RNS::info(msg))
#define INFOF(msg, ...) (RNS::infof(msg, __VA_ARGS__))
#define VERBOSE(msg) (RNS::verbose(msg))
#define VERBOSEF(msg, ...) (RNS::verbosef(msg, __VA_ARGS__))
#ifndef NDEBUG
	#define DEBUG(msg) (RNS::debug(msg))
	#define DEBUGF(msg, ...) (RNS::debugf(msg, __VA_ARGS__))
	#define TRACE(msg) (RNS::trace(msg))
	#define TRACEF(msg, ...) (RNS::tracef(msg, __VA_ARGS__))
	#if defined(MEM_LOG)
		#define MEM(msg) (RNS::mem(msg))
		#define MEMF(msg, ...) (RNS::memf(msg, __VA_ARGS__))
	#else
		#define MEM(ignore) ((void)0)
		#define MEMF(...) ((void)0)
	#endif
#else
	#define DEBUG(ignore) ((void)0)
	#define DEBUGF(...) ((void)0)
	#define TRACE(...) ((void)0)
	#define TRACEF(...) ((void)0)
	#define MEM(ignore) ((void)0)
	#define MEMF(...) ((void)0)
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

	void doLog(LogLevel level, const char* msg);
	inline void doLog(LogLevel level, const char* msg, va_list vlist) { char buf[1024]; vsnprintf(buf, sizeof(buf), msg, vlist); doLog(level, buf); }

	inline void log(const char* msg, LogLevel level = LOG_NOTICE) { doLog(level, msg); }
#ifdef ARDUINO
	inline void log(const String& msg, LogLevel level = LOG_NOTICE) { doLog(level, msg.c_str()); }
	inline void log(const StringSumHelper& msg, LogLevel level = LOG_NOTICE) { doLog(level, msg.c_str()); }
#endif
	inline void log(const std::string& msg, LogLevel level = LOG_NOTICE) { doLog(level, msg.c_str()); }
	inline void logf(LogLevel level, const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(level, msg, vlist); va_end(vlist); }

	void head(const char* msg, LogLevel level = LOG_NOTICE);
	inline void head(const std::string& msg, LogLevel level = LOG_NOTICE) { head(msg.c_str(), level); }
	inline void headf(LogLevel level, const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(level, msg, vlist); va_end(vlist); }

	inline void critical(const char* msg) { doLog(LOG_CRITICAL, msg); }
	inline void critical(const std::string& msg) { doLog(LOG_CRITICAL, msg.c_str()); }
	inline void criticalf(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_CRITICAL, msg, vlist); va_end(vlist); }

	inline void error(const char* msg) { doLog(LOG_ERROR, msg); }
	inline void error(const std::string& msg) { doLog(LOG_ERROR, msg.c_str()); }
	inline void errorf(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_ERROR, msg, vlist); va_end(vlist); }

	inline void warning(const char* msg) { doLog(LOG_WARNING, msg); }
	inline void warning(const std::string& msg) { doLog(LOG_WARNING, msg.c_str()); }
	inline void warningf(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_WARNING, msg, vlist); va_end(vlist); }

	inline void notice(const char* msg) { doLog(LOG_NOTICE, msg); }
	inline void notice(const std::string& msg) { doLog(LOG_NOTICE, msg.c_str()); }
	inline void noticef(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_NOTICE, msg, vlist); va_end(vlist); }

	inline void info(const char* msg) { doLog(LOG_INFO, msg); }
	inline void info(const std::string& msg) { doLog(LOG_INFO, msg.c_str()); }
	inline void infof(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_INFO, msg, vlist); va_end(vlist); }

	inline void verbose(const char* msg) { doLog(LOG_VERBOSE, msg); }
	inline void verbose(const std::string& msg) { doLog(LOG_VERBOSE, msg.c_str()); }
	inline void verbosef(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_VERBOSE, msg, vlist); va_end(vlist); }

	inline void debug(const char* msg) { doLog(LOG_DEBUG, msg); }
	inline void debug(const std::string& msg) { doLog(LOG_DEBUG, msg.c_str()); }
	inline void debugf(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_DEBUG, msg, vlist); va_end(vlist); }

	inline void trace(const char* msg) { doLog(LOG_TRACE, msg); }
	inline void trace(const std::string& msg) { doLog(LOG_TRACE, msg.c_str()); }
	inline void tracef(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_TRACE, msg, vlist); va_end(vlist); }

	inline void mem(const char* msg) { doLog(LOG_MEM, msg); }
	inline void mem(const std::string& msg) { doLog(LOG_MEM, msg.c_str()); }
	inline void memf(const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(LOG_MEM, msg, vlist); va_end(vlist); }

}
