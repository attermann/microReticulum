#include "Fernet.h"

#include "../Log.h"

using namespace RNS::Cryptography;

Fernet::Fernet() {
	log("Fernet object created", LOG_EXTREME);
}

Fernet::~Fernet() {
	log("Fernet object destroyed", LOG_EXTREME);
}

