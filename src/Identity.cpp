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
using namespace RNS::Type::Identity;

Identity::Identity(bool create_keys) : _object(new Object()) {
	if (create_keys) {
		createKeys();
	}
	extreme("Identity object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
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


/*static*/ bool Identity::validate_announce(const Packet &packet) {
/*
	try:
		if packet.packet_type == RNS.Packet.ANNOUNCE:
			destination_hash = packet.destination_hash
			public_key = packet.data[:Identity.KEYSIZE//8]
			name_hash = packet.data[Identity.KEYSIZE//8:Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8]
			random_hash = packet.data[Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8:Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8+10]
			signature = packet.data[Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8+10:Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8+10+Identity.SIGLENGTH//8]
			app_data = b""
			if len(packet.data) > Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8+10+Identity.SIGLENGTH//8:
				app_data = packet.data[Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8+10+Identity.SIGLENGTH//8:]

			signed_data = destination_hash+public_key+name_hash+random_hash+app_data

			if not len(packet.data) > Identity.KEYSIZE//8+Identity.NAME_HASH_LENGTH//8+10+Identity.SIGLENGTH//8:
				app_data = None

			announced_identity = Identity(create_keys=False)
			announced_identity.load_public_key(public_key)

			if announced_identity.pub != None and announced_identity.validate(signature, signed_data):
				hash_material = name_hash+announced_identity.hash
				expected_hash = RNS.Identity.full_hash(hash_material)[:RNS.Reticulum.TRUNCATED_HASHLENGTH//8]

				if destination_hash == expected_hash:
					# Check if we already have a public key for this destination
					# and make sure the public key is not different.
					if destination_hash in Identity.known_destinations:
						if public_key != Identity.known_destinations[destination_hash][2]:
							# In reality, this should never occur, but in the odd case
							# that someone manages a hash collision, we reject the announce.
							RNS.log("Received announce with valid signature and destination hash, but announced public key does not match already known public key.", RNS.LOG_CRITICAL)
							RNS.log("This may indicate an attempt to modify network paths, or a random hash collision. The announce was rejected.", RNS.LOG_CRITICAL)
							return False

					RNS.Identity.remember(packet.get_hash(), destination_hash, public_key, app_data)
					del announced_identity

					if packet.rssi != None or packet.snr != None:
						signal_str = " ["
						if packet.rssi != None:
							signal_str += "RSSI "+str(packet.rssi)+"dBm"
							if packet.snr != None:
								signal_str += ", "
						if packet.snr != None:
							signal_str += "SNR "+str(packet.snr)+"dB"
						signal_str += "]"
					else:
						signal_str = ""

					if hasattr(packet, "transport_id") and packet.transport_id != None:
						RNS.log("Valid announce for "+RNS.prettyhexrep(destination_hash)+" "+str(packet.hops)+" hops away, received via "+RNS.prettyhexrep(packet.transport_id)+" on "+str(packet.receiving_interface)+signal_str, RNS.LOG_EXTREME)
					else:
						RNS.log("Valid announce for "+RNS.prettyhexrep(destination_hash)+" "+str(packet.hops)+" hops away, received on "+str(packet.receiving_interface)+signal_str, RNS.LOG_EXTREME)

					return True

				else:
					RNS.log("Received invalid announce for "+RNS.prettyhexrep(destination_hash)+": Destination mismatch.", RNS.LOG_DEBUG)
					return False

			else:
				RNS.log("Received invalid announce for "+RNS.prettyhexrep(destination_hash)+": Invalid signature.", RNS.LOG_DEBUG)
				del announced_identity
				return False
	
	except Exception as e:
		RNS.log("Error occurred while validating announce. The contained exception was: "+str(e), RNS.LOG_ERROR)
		return False
*/
	// MOCK
	return true;
}

/*
Encrypts information for the identity.

:param plaintext: The plaintext to be encrypted as *bytes*.
:returns: Ciphertext token as *bytes*.
:raises: *KeyError* if the instance does not hold a public key.
*/
const Bytes Identity::encrypt(const Bytes &plaintext) {
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
const Bytes Identity::decrypt(const Bytes &ciphertext_token) {
	assert(_object);
	debug("Identity::decrypt: decrypting data...");
	if (!_object->_prv) {
		throw std::runtime_error("Decryption failed because identity does not hold a private key");
	}
	if (ciphertext_token.size() <= Type::Identity::KEYSIZE/8/2) {
		debug("Decryption failed because the token size " + std::to_string(ciphertext_token.size()) + " was invalid.");
		return {Bytes::NONE};
	}
	Bytes plaintext;
	try {
		//peer_pub_bytes = ciphertext_token[:Identity.KEYSIZE//8//2]
		Bytes peer_pub_bytes = ciphertext_token.left(Type::Identity::KEYSIZE/8/2);
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
		Bytes ciphertext(ciphertext_token.mid(Type::Identity::KEYSIZE/8/2));
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
const Bytes Identity::sign(const Bytes &message) {
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

void Identity::prove(const Packet &packet, const Destination &destination /*= {Type::NONE}*/) {
	assert(_object);
	Bytes signature(sign(packet.packet_hash()));
	Bytes proof_data;
	if (RNS::Reticulum::should_use_implicit_proof()) {
		proof_data = signature;
	}
	else {
		proof_data = packet.packet_hash() + signature;
	}
	
	//z if (!destination) {
	//z		destination = packet.generate_proof_destination();
	//z }

	Packet proof(destination, packet.receiving_interface(), proof_data, Type::Packet::PROOF);
	proof.send();
}

void Identity::prove(const Packet &packet) {
	prove(packet, {Type::NONE});
}
