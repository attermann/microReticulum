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

	_object->_prv           = Cryptography::X25519PrivateKey::generate();
	_object->_prv_bytes     = _object->_prv->private_bytes();
	debug("Identity::createKeys: prv bytes: " + _object->_prv_bytes.toHex());

	_object->_sig_prv       = Cryptography::Ed25519PrivateKey::generate();
	_object->_sig_prv_bytes = _object->_sig_prv->private_bytes();
	debug("Identity::createKeys: sig prv bytes: " + _object->_sig_prv_bytes.toHex());

	_object->_pub           = _object->_prv->public_key();
	_object->_pub_bytes     = _object->_pub->public_bytes();
	debug("Identity::createKeys: pub bytes: " + _object->_pub_bytes.toHex());

	_object->_sig_pub       = _object->_sig_prv->public_key();
	_object->_sig_pub_bytes = _object->_sig_pub->public_bytes();
	debug("Identity::createKeys: sig pub bytes: " + _object->_sig_pub_bytes.toHex());

	update_hashes();

	verbose("Identity keys created for " + _object->_hash.toHex());
}

/*
:returns: The public key as *bytes*
*/
Bytes Identity::get_public_key() {
	assert(_object);
	return _object->_pub_bytes + _object->_sig_pub_bytes;
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
	if (_object->_sig_prv) {
		try {
			return _object->_sig_prv->sign(message);
		}
		catch (std::exception e) {
			error("The identity " + toString() + " could not sign the requested message. The contained exception was: " + e.what());
			throw e;
		}
	}
	else {
		throw std::runtime_error("Signing failed because identity does not hold a private key");
	}
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
