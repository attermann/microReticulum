#include "Test.h"

#include "Log.h"

void test() {

	//RNS::LogLevel loglevel = RNS::loglevel();
	//RNS::loglevel(RNS::LOG_WARNING);

	testBytes();
	testCowBytes();
	testBytesConversion();
	testMap();

	testReference();

	testCrypto();
	testHMAC();
	testPKCS7();

	//RNS::loglevel(loglevel);

}
