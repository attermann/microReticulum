#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stdarg.h>

#include <string>

#ifndef ARDUINO
	#define msg (msg)
#endif

#define LOG(msg, level) (RNS::log(msg, level))
#define LOGF(level, msg, ...) (RNS::logf(level, msg, __VA_ARGS__))
#define HEAD(msg, level) (RNS::head(msg, level))
#define HEADF(level, msg, ...) (RNS::headf(level, msg, __VA_ARGS__))

#define CRITICAL(msg) (RNS::log(msg, RNS::LOG_CRITICAL))
#define CRITICALF(msg, ...) (RNS::logf(RNS::LOG_CRITICAL, msg, __VA_ARGS__))
#define ERROR(msg) (RNS::log(msg, RNS::LOG_ERROR))
#define ERRORF(msg, ...) (RNS::logf(RNS::LOG_ERROR, msg, __VA_ARGS__))
#define WARNING(msg) (RNS::log(msg, RNS::LOG_WARNING))
#define WARNINGF(msg, ...) (RNS::logf(RNS::LOG_WARNING, msg, __VA_ARGS__))
#define NOTICE(msg) (RNS::log(msg, RNS::LOG_NOTICE))
#define NOTICEF(msg, ...) (RNS::logf(RNS::LOG_NOTICE, msg, __VA_ARGS__))
#define INFO(msg) (RNS::log(msg, RNS::LOG_INFO))
#define INFOF(msg, ...) (RNS::logf(RNS::LOG_INFO, msg, __VA_ARGS__))
#define VERBOSE(msg) (RNS::log(msg, RNS::LOG_VERBOSE))
#define VERBOSEF(msg, ...) (RNS::logf(RNS::LOG_VERBOSE, msg, __VA_ARGS__))
#ifndef NDEBUG
	#define DEBUG(msg) (RNS::log(msg, RNS::LOG_DEBUG))
	#define DEBUGF(msg, ...) (RNS::logf(RNS::LOG_DEBUG, msg, __VA_ARGS__))
	#define TRACE(msg) (RNS::log(msg, RNS::LOG_TRACE))
	#define TRACEF(msg, ...) (RNS::logf(RNS::LOG_TRACE, msg, __VA_ARGS__))
	#if defined(RNS_MEM_LOG)
		#define MEM(msg) (RNS::log(msg, RNS::LOG_MEM))
		#define MEMF(msg, ...) (RNS::logf(RNS::LOG_MEM, msg, __VA_ARGS__))
	#else
		#define MEM(ignore) ((void)0)
		#define MEMF(...) ((void)0)
	#endif
#else
	#define DEBUG(ignore) ((void)0)
	#define DEBUGF(...) ((void)0)
	#define TRACE(ignore) ((void)0)
	#define TRACEF(...) ((void)0)
	#define MEM(ignore) ((void)0)
	#define MEMF(...) ((void)0)
#endif

#define RNS_LOG_BUFFER_SIZE 1024

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
	inline void doLog(LogLevel level, const char* msg, va_list vlist) { char buf[RNS_LOG_BUFFER_SIZE]; vsnprintf(buf, sizeof(buf), msg, vlist); doLog(level, buf); }
#ifdef ARDUINO
	//inline void doLog(LogLevel level, const __FlashStringHelper* msg) { char buf[RNS_LOG_BUFFER_SIZE]; strncpy_P(buf, (const char*)msg, sizeof(buf)); doLog(level, buf); }
	//inline void doLog(LogLevel level, const __FlashStringHelper* msg, va_list vlist) { char buf[RNS_LOG_BUFFER_SIZE]; vsnprintf_P(buf, sizeof(buf), (const char*)msg, vlist); doLog(level, buf); }
#endif

	void doHeadLog(LogLevel level, const char* msg);
	inline void doHeadLog(LogLevel level, const char* msg, va_list vlist) { char buf[RNS_LOG_BUFFER_SIZE]; vsnprintf(buf, sizeof(buf), msg, vlist); doHeadLog(level, buf); }
#ifdef ARDUINO
	//inline void doHeadLog(LogLevel level, const __FlashStringHelper* msg) { char buf[RNS_LOG_BUFFER_SIZE]; strncpy_P(buf, (const char*)msg, sizeof(buf)); doHeadLog(level, buf); }
	//inline void doHeadLog(LogLevel level, const __FlashStringHelper* msg, va_list vlist) { char buf[RNS_LOG_BUFFER_SIZE]; vsnprintf_P(buf, sizeof(buf), (const char*)msg, vlist); doHeadLog(level, buf); }
#endif

	inline void log(const char* msg, LogLevel level = LOG_NOTICE) { doLog(level, msg); }
	inline void log(const std::string& msg, LogLevel level = LOG_NOTICE) { doLog(level, msg.c_str()); }
	inline void logf(LogLevel level, const char* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(level, msg, vlist); va_end(vlist); }
#ifdef ARDUINO
	inline void log(const String& msg, LogLevel level = LOG_NOTICE) { doLog(level, msg.c_str()); }
	inline void log(const StringSumHelper& msg, LogLevel level = LOG_NOTICE) { doLog(level, msg.c_str()); }
	//inline void log(const __FlashStringHelper* msg, LogLevel level = LOG_NOTICE) { doLog(level, msg); }
	//inline void logf(LogLevel level, const __FlashStringHelper* msg, ...) { va_list vlist; va_start(vlist, msg); doLog(level, msg, vlist); va_end(vlist); }
#endif

	inline void head(const char* msg, LogLevel level = LOG_NOTICE) { doHeadLog(level, msg); }
	inline void head(const std::string& msg, LogLevel level = LOG_NOTICE) { doHeadLog(level, msg.c_str()); }
	inline void headf(LogLevel level, const char* msg, ...) { va_list vlist; va_start(vlist, msg); doHeadLog(level, msg, vlist); va_end(vlist); }
#ifdef ARDUINO
	inline void head(const String& msg, LogLevel level = LOG_NOTICE) { doHeadLog(level, msg.c_str()); }
	inline void head(const StringSumHelper& msg, LogLevel level = LOG_NOTICE) { doHeadLog(level, msg.c_str()); }
	//inline void head(const __FlashStringHelper* msg, LogLevel level = LOG_NOTICE) { doHeadLog(level, msg); }
	//inline void headf(LogLevel level, const __FlashStringHelper* msg, ...) { va_list vlist; va_start(vlist, msg); doHeadLog(level, msg, vlist); va_end(vlist); }
#endif

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
