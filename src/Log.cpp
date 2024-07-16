#include "Log.h"

#include "Utilities/OS.h"

#include <sys/time.h>
#include <time.h>
#include <stdio.h>

using namespace RNS;

//LogLevel _level = LOG_VERBOSE;
LogLevel _level = LOG_TRACE;
//LogLevel _level = LOG_MEM;
RNS::log_callback _on_log = nullptr;
char _datetime[20];

const char* RNS::getLevelName(LogLevel level) {
	switch (level) {
	case LOG_CRITICAL:
		return "!!!";
	case LOG_ERROR:
		return "ERR";
	case LOG_WARNING:
		return "WRN";
	case LOG_NOTICE:
		return "NOT";
	case LOG_INFO:
		return "INF";
	case LOG_VERBOSE:
		return "VRB";
	case LOG_DEBUG:
		return "DBG";
	case LOG_TRACE:
		return "---";
	case LOG_MEM:
		return "...";
	default:
		return "";
	}
}

const char* RNS::getTimeString() {
#ifdef ARDUINO
	uint64_t time = Utilities::OS::ltime();
	if (time < 86400000) {
		snprintf(_datetime, sizeof(_datetime), "%02d:%02d:%02d.%03d", (int)(time/3600000), (int)((time/60000)%60), (int)((time/1000)%60), (int)(time%1000));
	}
	else {
		snprintf(_datetime, sizeof(_datetime), "%02d-%02d:%02d:%02d.%03d", (int)(time/86400000), (int)((time/3600000)%24), (int)((time/60000)%60), (int)((time/1000)%60), (int)(time%1000));
	}
	return _datetime;
#else
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	int millis = lrint(tv.tv_usec/1000.0);
	if (millis >= 1000) {
		millis -= 1000;
		tv.tv_sec++;
	}
	struct tm* tm = localtime(&tv.tv_sec);
	size_t len = strftime(_datetime, sizeof(_datetime), "%Y-%m-%d %H:%M:%S", tm);
	snprintf(_datetime+len, sizeof(_datetime)-len, ".%03d", millis);
	return _datetime;
#endif
}

void RNS::loglevel(LogLevel level) {
	_level = level;
}

LogLevel RNS::loglevel() {
	return _level;
}

void RNS::setLogCallback(log_callback on_log /*= nullptr*/) {
	_on_log = on_log;
}

void RNS::doLog(LogLevel level, const char* msg) {
	if (level > _level) {
		return;
	}
	if (_on_log != nullptr) {
		_on_log(msg, level);
		return;
	}
#ifdef ARDUINO
	Serial.print(getTimeString());
	Serial.print(" [");
	Serial.print(getLevelName(level));
	Serial.print("] ");
	Serial.println(msg);
	Serial.flush();
#else
	printf("%s [%s] %s\n", getTimeString(), getLevelName(level), msg);
#endif
}

void HEAD(const char* msg, LogLevel level) {
	if (level > _level) {
		return;
	}
#ifdef ARDUINO
	Serial.println("");
#else
	printf("\n");
#endif
	doLog(level, msg);
}
