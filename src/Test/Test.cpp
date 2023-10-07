#include "Test.h"

#include "Log.h"

void test() {

	//RNS::LogLevel loglevel = RNS::loglevel();
	//RNS::loglevel(RNS::LOG_WARNING);

	testMap();
	testBytes();
	testCowBytes();
	testObjects();
	testBytesConversion();

	testReference();

	testCrypto();

	//RNS::loglevel(loglevel);

}
