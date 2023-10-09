#include "Identity.h"

#include "Reticulum.h"
#include "Packet.h"
#include "Log.h"
#include "Cryptography/X25519.h"
#include "Cryptography/HKDF.h"
#include "Cryptography/Fernet.h"
#include "Cryptography/Random.h"

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

	// CRYPTO: create encryption private keys
	_object->_prv           = Cryptography::X25519PrivateKey::generate();
	_object->_prv_bytes     = _object->_prv->private_bytes();
	debug("Identity::createKeys: prv bytes:     " + _object->_prv_bytes.toHex());

	// CRYPTO: create encryption public keys
	_object->_pub           = _object->_prv->public_key();
	_object->_pub_bytes     = _object->_pub->public_bytes();
	debug("Identity::createKeys: pub bytes:     " + _object->_pub_bytes.toHex());

	// CRYPTO: create signature private keys
	_object->_sig_prv       = Cryptography::Ed25519PrivateKey::generate();
	_object->_sig_prv_bytes = _object->_sig_prv->private_bytes();
	debug("Identity::createKeys: sig prv bytes: " + _object->_sig_prv_bytes.toHex());

	// CRYPTO: create signature public keys
	_object->_sig_pub       = _object->_sig_prv->public_key();
	_object->_sig_pub_bytes = _object->_sig_pub->public_bytes();
	debug("Identity::createKeys: sig pub bytes: " + _object->_sig_pub_bytes.toHex());

	update_hashes();

	verbose("Identity keys created for " + _object->_hash.toHex());
}


/*
Encrypts information for the identity.

:param plaintext: The plaintext to be encrypted as *bytes*.
:returns: Ciphertext token as *bytes*.
:raises: *KeyError* if the instance does not hold a public key.
*/
Bytes Identity::encrypt(const Bytes &plaintext) {
	assert(_object);
	debug("Identity::encrypt: encrypting data...");
	if (!_object->_pub) {
		throw std::runtime_error("Encryption failed because identity does not hold a public key");
	}
	Cryptography::X25519PrivateKey::Ptr ephemeral_key = Cryptography::X25519PrivateKey::generate();
	Bytes ephemeral_pub_bytes = ephemeral_key->public_key()->public_bytes();
	debug("Identity::encrypt: ephemeral public key: " + ephemeral_pub_bytes.toHex());

	// CRYPTO: create shared key for key exchange using own public key
	//shared_key = ephemeral_key.exchange(self.pub)
	Bytes shared_key = ephemeral_key->exchange(_object->_pub_bytes);
	debug("Identity::encrypt: shared key:           " + shared_key.toHex());

	Bytes derived_key = Cryptography::hkdf(
		32,
		shared_key,
		get_salt(),
		get_context()
	);
	debug("Identity::encrypt: derived key:          " + derived_key.toHex());

	Cryptography::Fernet fernet(derived_key);
	debug("Identity::encrypt: Fernet encrypting data of length " + std::to_string(plaintext.size()));
	extreme("Identity::encrypt: plaintext:  " + plaintext.toHex());
	Bytes ciphertext = fernet.encrypt(plaintext);
	extreme("Identity::encrypt: ciphertext: " + ciphertext.toHex());

	return ephemeral_pub_bytes + ciphertext;
}


/*
Decrypts information for the identity.

:param ciphertext: The ciphertext to be decrypted as *bytes*.
:returns: Plaintext as *bytes*, or *None* if decryption fails.
:raises: *KeyError* if the instance does not hold a private key.
*/
Bytes Identity::decrypt(const Bytes &ciphertext_token) {
	assert(_object);
	debug("Identity::decrypt: decrypting data...");
	if (!_object->_prv) {
		throw std::runtime_error("Decryption failed because identity does not hold a private key");
	}
	if (ciphertext_token.size() <= Identity::KEYSIZE/8/2) {
		debug("Decryption failed because the token size " + std::to_string(ciphertext_token.size()) + " was invalid.");
		return Bytes::NONE;
	}
	Bytes plaintext;
	try {
		//peer_pub_bytes = ciphertext_token[:Identity.KEYSIZE//8//2]
		Bytes peer_pub_bytes = ciphertext_token.left(Identity::KEYSIZE/8/2);
		//peer_pub = X25519PublicKey.from_public_bytes(peer_pub_bytes)
		//Cryptography::X25519PublicKey::Ptr peer_pub = Cryptography::X25519PublicKey::from_public_bytes(peer_pub_bytes);
		debug("Identity::decrypt: peer public key:      " + peer_pub_bytes.toHex());

		// CRYPTO: create shared key for key exchange using peer public key
		//shared_key = _object->_prv->exchange(peer_pub);
		Bytes shared_key = _object->_prv->exchange(peer_pub_bytes);
		debug("Identity::decrypt: shared key:           " + shared_key.toHex());

		Bytes derived_key = Cryptography::hkdf(
			32,
			shared_key,
			get_salt(),
			get_context()
		);
		debug("Identity::decrypt: derived key:          " + derived_key.toHex());

		Cryptography::Fernet fernet(derived_key);
		//ciphertext = ciphertext_token[Identity.KEYSIZE//8//2:]
		Bytes ciphertext(ciphertext_token.mid(Identity::KEYSIZE/8/2));
		debug("Identity::decrypt: Fernet decrypting data of length " + std::to_string(ciphertext.size()));
		extreme("Identity::decrypt: ciphertext: " + ciphertext.toHex());
		plaintext = fernet.decrypt(ciphertext);
		extreme("Identity::decrypt: plaintext:  " + plaintext.toHex());
		debug("Identity::decrypt: Fernet decrypted data of length " + std::to_string(plaintext.size()));
	}
	catch (std::exception &e) {
		debug("Decryption by " + toString() + " failed: " + e.what());
	}
		
	return plaintext;
}

/*
Signs information by the identity.

:param message: The message to be signed as *bytes*.
:returns: Signature as *bytes*.
:raises: *KeyError* if the instance does not hold a private key.
*/
Bytes Identity::sign(const Bytes &message) {
	assert(_object);
	if (!_object->_sig_prv) {
		throw std::runtime_error("Signing failed because identity does not hold a private key");
	}
	try {
		return _object->_sig_prv->sign(message);
	}
	catch (std::exception &e) {
		error("The identity " + toString() + " could not sign the requested message. The contained exception was: " + e.what());
		throw e;
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
