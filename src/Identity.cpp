#include "Identity.h"

#include "Reticulum.h"
#include "Packet.h"
#include "Log.h"
#include "Cryptography/Hashes.h"
#include "Cryptography/X25519.h"

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
	debug("Identity::createKeys: prv bytes:     " + _object->_prv_bytes.toHex());

	_object->_sig_prv       = Cryptography::Ed25519PrivateKey::generate();
	_object->_sig_prv_bytes = _object->_sig_prv->private_bytes();
	debug("Identity::createKeys: sig prv bytes: " + _object->_sig_prv_bytes.toHex());

	_object->_pub           = _object->_prv->public_key();
	_object->_pub_bytes     = _object->_pub->public_bytes();
	debug("Identity::createKeys: pub bytes:     " + _object->_pub_bytes.toHex());

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
}

void Identity::update_hashes() {
	assert(_object);
	_object->_hash = truncated_hash(get_public_key());
	debug("Identity::update_hashes: hash:    " + _object->_hash.toHex());
	_object->_hexhash = _object->_hash.toHex();
	debug("Identity::update_hashes: hexhash: " + _object->_hexhash);
};


/*
Get a SHA-256 hash of passed data.

:param data: Data to be hashed as *bytes*.
:returns: SHA-256 hash as *bytes*
*/
/*static*/ Bytes Identity::full_hash(const Bytes &data) {
	return Cryptography::sha256(data);
}

/*
Get a truncated SHA-256 hash of passed data.

:param data: Data to be hashed as *bytes*.
:returns: Truncated SHA-256 hash as *bytes*
*/
/*static*/ Bytes Identity::truncated_hash(const Bytes &data) {
	return full_hash(data).right(TRUNCATED_HASHLENGTH/8);
}


/*
Encrypts information for the identity.

:param plaintext: The plaintext to be encrypted as *bytes*.
:returns: Ciphertext token as *bytes*.
:raises: *KeyError* if the instance does not hold a public key.
*/
Bytes Identity::encrypt(const Bytes &plaintext) {
	assert(_object);
	if (_object->_pub) {
		Cryptography::X25519PrivateKey::Ptr ephemeral_key = Cryptography::X25519PrivateKey::generate();
		Bytes ephemeral_pub_bytes = ephemeral_key->public_key()->public_bytes();

/*
		Bytes shared_key = ephemeral_key->exchange(_object->_pub);

		Bytes derived_key = RNS.Cryptography.hkdf(
			length=32,
			derive_from=shared_key,
			salt=get_salt(),
			context=get_context(),
		)

		fernet = Fernet(derived_key)
		ciphertext = fernet.encrypt(plaintext)

		return ephemeral_pub_bytes + ciphertext;
*/
		// MOCK
		return Bytes::NONE;
	}
	else {
		throw std::runtime_error("Encryption failed because identity does not hold a public key");
	}
}


/*
Decrypts information for the identity.

:param ciphertext: The ciphertext to be decrypted as *bytes*.
:returns: Plaintext as *bytes*, or *None* if decryption fails.
:raises: *KeyError* if the instance does not hold a private key.
*/
Bytes Identity::decrypt(const Bytes &ciphertext_token) {
	assert(_object);
	if (_object->_prv) {
		if (ciphertext_token.size() > Identity::KEYSIZE/8/2) {
			Bytes plaintext;
			try {
				Bytes peer_pub_bytes = ciphertext_token.right(Identity::KEYSIZE/8/2);
				Cryptography::X25519PublicKey::Ptr peer_pub = Cryptography::X25519PublicKey::from_public_bytes(peer_pub_bytes);

/*
				Bytes shared_key = _object->_prv->exchange(peer_pub);

				Bytes derived_key = RNS.Cryptography.hkdf(
					length=32,
					derive_from=shared_key,
					salt=get_salt(),
					context=get_context(),
				)

				fernet = Fernet(derived_key)
				ciphertext = ciphertext_token[Identity.KEYSIZE//8//2:]
				plaintext = fernet.decrypt(ciphertext)
*/
			}
			catch (std::exception &e) {
				debug("Decryption by " + _object->_hash.toHex() + " failed: " + e.what());
			}
				
			return plaintext;
		}
		else {
			debug("Decryption failed because the token size was invalid.");
			return Bytes::NONE;
		}
	}
	else {
		throw std::runtime_error("Decryption failed because identity does not hold a private key");
	}
}

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
		catch (std::exception &e) {
			error("The identity " + toString() + " could not sign the requested message. The contained exception was: " + e.what());
			throw e;
		}
	}
	else {
		throw std::runtime_error("Signing failed because identity does not hold a private key");
	}
}

/*
Validates the signature of a signed message.

:param signature: The signature to be validated as *bytes*.
:param message: The message to be validated as *bytes*.
:returns: True if the signature is valid, otherwise False.
:raises: *KeyError* if the instance does not hold a public key.
*/
bool Identity::validate(const Bytes &signature, const Bytes &message) {
	assert(_object);
	if (_object->_pub) {
		try {
			_object->_sig_pub->verify(signature, message);
			return true;
		}
		catch (std::exception &e) {
			return false;
		}
	}
	else {
		throw std::runtime_error("Signature validation failed because identity does not hold a public key");
	}
}

void Identity::prove(const Packet &packet, const Destination &destination /*= Destination::NONE*/) {
	assert(_object);
	Bytes signature(sign(packet._packet_hash));
	Bytes proof_data;
	if (RNS::Reticulum::should_use_implicit_proof()) {
		proof_data = signature;
	}
	else {
		proof_data = packet._packet_hash + signature;
	}
	
	//zif (!destination) {
	//z	destination = packet.generate_proof_destination();
	//z}

	Packet proof(destination, packet.receiving_interface(), proof_data, RNS::Packet::PROOF);
	proof.send();
}
