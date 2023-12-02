#include "Log.h"

#include <stdio.h>

using namespace RNS;

const char* getLevelName(LogLevel level) {
	switch (level) {
	case LOG_CRITICAL:
		return "CRITICAL";
	case LOG_ERROR:
		return "ERROR";
	case LOG_WARNING:
		return "WARNING";
	case LOG_NOTICE:
		return "NOTICE";
	case LOG_INFO:
		return "INFO";
	case LOG_VERBOSE:
		return "VERBOSE";
	case LOG_DEBUG:
		return "DEBUG";
	case LOG_EXTREME:
		return "EXTRA";
	case LOG_MEM:
		return "MEM";
	default:
		return "";
	}
}

//LogLevel _level = LOG_VERBOSE;
LogLevel _level = LOG_EXTREME;
//LogLevel _level = LOG_MEM;

void RNS::loglevel(LogLevel level) {
	_level = level;
}

LogLevel RNS::loglevel() {
	return _level;
}

void RNS::doLog(const char* msg, LogLevel level) {
	if (level > _level) {
		return;
	}
#ifdef ARDUINO
	Serial.print(getLevelName(level));
	Serial.print(" ");
	Serial.println(msg);
	Serial.flush();
#else
	printf("%s: %s\n", getLevelName(level), msg);
#endif
}

void RNS::head(const char* msg, LogLevel level) {
	if (level > _level) {
		return;
	}
#ifdef ARDUINO
	Serial.println("");
#else
	printf("\n");
#endif
	doLog(msg, level);
}
