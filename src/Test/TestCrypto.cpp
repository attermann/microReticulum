#include <unity.h>

#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Bytes.h"

void testCrypto() {

	RNS::Reticulum reticulum;

	RNS::Identity identity;

	RNS::Destination destination(identity, RNS::Destination::IN, RNS::Destination::SINGLE, "appname", "aspects");
	//assert(encryptionPrivateKey().toHex().compare("") == );
	//assert(signingPrivateKey().toHex().compare("") == );
	//assert(encryptionPublicKey().toHex().compare("") == );
	//assert(signingPublicKey().toHex().compare("") == );

	//Packet packet = destination.announce("appdata");
}

/*
int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(testCrypto);
	return UNITY_END();
}
*/
