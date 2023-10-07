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
	default:
		return "";
	}
}

LogLevel _level = LOG_VERBOSE;

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
#ifndef NATIVE
	Serial.print(getLevelName(level));
	Serial.print(" ");
	Serial.println(msg);
#else
	printf("%s: %s\n", getLevelName(level), msg);
#endif
}
