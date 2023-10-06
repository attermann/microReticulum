#include "Identity.h"

#include "Log.h"
#include "Cryptography/Hashes.h"

#include <string.h>

using namespace RNS;

Identity::Identity(bool create_keys) : _object(new Object()) {
	if (create_keys) {
		createKeys();
	}
	extreme("Identity object created, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((ulong)_object.get()));
}


void Identity::createKeys() {
	assert(_object);

/*
	self.prv           = X25519PrivateKey.generate()
	self.prv_bytes     = self.prv.private_bytes()

	self.sig_prv       = Ed25519PrivateKey.generate()
	self.sig_prv_bytes = self.sig_prv.private_bytes()

	self.pub           = self.prv.public_key()
	self.pub_bytes     = self.pub.public_bytes()

	self.sig_pub       = self.sig_prv.public_key()
	self.sig_pub_bytes = self.sig_pub.public_bytes()
*/

	update_hashes();

	//verbose("Identity keys created for " + _object->_hash.toHex());
}

Bytes Identity::get_public_key() {
	assert(_object);
	// MOCK
	return "abc123";
}

void Identity::update_hashes() {
	assert(_object);
	_object->_hash = truncated_hash(get_public_key());
	debug("Identity::update_hashes: hash: " + _object->_hash.toHex());
	_object->_hexhash = _object->_hash.toHex();
	debug("Identity::update_hashes: hexhash: " + _object->_hexhash);
};

/*
Signs information by the identity.

:param message: The message to be signed as *bytes*.
:returns: Signature as *bytes*.
:raises: *KeyError* if the instance does not hold a private key.
*/
Bytes Identity::sign(const Bytes &message) {
	assert(_object);
/*
	if self.sig_prv != None:
		try:
			return self.sig_prv.sign(message)    
		except Exception as e:
			RNS.log("The identity "+str(self)+" could not sign the requested message. The contained exception was: "+str(e), RNS.LOG_ERROR)
			raise e
	else:
		raise KeyError("Signing failed because identity does not hold a private key")
*/
	// MOCK
	return {message};
}


/*
Get a SHA-256 hash of passed data.

:param data: Data to be hashed as *bytes*.
:returns: SHA-256 hash as *bytes*
*/
/*static*/ Bytes Identity::full_hash(const Bytes &data) {
	Bytes hash;
	Cryptography::sha256(hash.writable(HASHLENGTH/8), data.data(), data.size());
	//debug("Identity::full_hash: hash: " + hash.toHex());
	return hash;
}

/*
Get a truncated SHA-256 hash of passed data.

:param data: Data to be hashed as *bytes*.
:returns: Truncated SHA-256 hash as *bytes*
*/
/*static*/ Bytes Identity::truncated_hash(const Bytes &data) {
	Bytes hash = full_hash(data);
	//Bytes truncated_hash(hash.data() + (TRUNCATED_HASHLENGTH/8), TRUNCATED_HASHLENGTH/8);
	//debug("Identity::truncated_hash: truncated hash: " + truncated_hash.toHex());
	return Bytes(hash.data() + (TRUNCATED_HASHLENGTH/8), TRUNCATED_HASHLENGTH/8);
}
