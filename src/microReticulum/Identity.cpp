/*
 * Copyright (c) 2023 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "Identity.h"

#include "Reticulum.h"
#include "Transport.h"
#include "Packet.h"
#include "Log.h"
#include "Utilities/OS.h"
#include "Cryptography/Ed25519.h"
#include "Cryptography/X25519.h"
#include "Cryptography/HKDF.h"
#include "Cryptography/Token.h"
#include "Cryptography/Random.h"

#include <algorithm>
#include <string.h>

using namespace RNS;
using namespace RNS::Type::Identity;
using namespace RNS::Cryptography;
using namespace RNS::Utilities;

#ifndef RNS_KNOWN_DESTINATIONS_MAX
#define RNS_KNOWN_DESTINATIONS_MAX 100
#endif

#ifndef RNS_KNOWN_DESTINATIONS_SEGMENT_SIZE
#define RNS_KNOWN_DESTINATIONS_SEGMENT_SIZE 65536
#endif

#ifndef RNS_KNOWN_DESTINATIONS_SEGMENT_COUNT
#define RNS_KNOWN_DESTINATIONS_SEGMENT_COUNT 8
#endif

#ifndef RNS_IDENTITY_ANNOUNCE_RECALL
#define RNS_IDENTITY_ANNOUNCE_RECALL 1
#endif


/*static*/ uint16_t Identity::_known_destinations_maxsize = RNS_KNOWN_DESTINATIONS_MAX;
/*static*/ uint32_t Identity::_known_store_segment_size = 0;
/*static*/ uint8_t Identity::_known_store_segment_count = 0;
#if defined(RNS_USE_FS) && RNS_PERSIST_KNOWN_DESTINATIONS
/*static*/ Persistence::KnownStore Identity::_known_store(RNS_KNOWN_DESTINATIONS_SEGMENT_SIZE, RNS_KNOWN_DESTINATIONS_SEGMENT_COUNT);
#else
/*static*/ Persistence::KnownStore Identity::_known_store;
#endif
/*static*/ Persistence::KnownDestinations Identity::_known_destinations(Identity::_known_store);

Identity::Identity(bool create_keys /*= true*/) : _object(new Object()) {
	if (create_keys) {
		createKeys();
	}
	MEMF("Identity object created, this: %p, data: %p", (void*)this, (void*)_object.get());
}

void Identity::createKeys() {
	assert(_object);

	// CRYPTO: create encryption private keys
	_object->_prv           = Cryptography::X25519PrivateKey::generate();
	assert(_object->_prv);
	_object->_prv_bytes     = _object->_prv->private_bytes();
	//TRACEF("Identity::createKeys: prv bytes:     %s", _object->_prv_bytes.toHex().c_str());

	// CRYPTO: create signature private keys
	_object->_sig_prv       = Cryptography::Ed25519PrivateKey::generate();
	assert(_object->_sig_prv);
	_object->_sig_prv_bytes = _object->_sig_prv->private_bytes();
	//TRACEF("Identity::createKeys: sig prv bytes: %s", _object->_sig_prv_bytes.toHex().c_str());

	// CRYPTO: create encryption public keys
	_object->_pub           = _object->_prv->public_key();
	assert(_object->_pub);
	_object->_pub_bytes     = _object->_pub->public_bytes();
	//TRACEF("Identity::createKeys: pub bytes:     %s", _object->_pub_bytes.toHex().c_str());

	// CRYPTO: create signature public keys
	_object->_sig_pub       = _object->_sig_prv->public_key();
	assert(_object->_sig_pub);
	_object->_sig_pub_bytes = _object->_sig_pub->public_bytes();
	//TRACEF("Identity::createKeys: sig pub bytes: %s", _object->_sig_pub_bytes.toHex().c_str());

	update_hashes();

	VERBOSEF("Identity keys created for %s", _object->_hash.toHex().c_str());
}

/*
Load a private key into the instance.

:param prv_bytes: The private key as *bytes*.
:returns: True if the key was loaded, otherwise False.
*/
bool Identity::load_private_key(const Bytes& prv_bytes) {
	assert(_object);

	try {

		//p self.prv_bytes     = prv_bytes[:Identity.KEYSIZE//8//2]
		_object->_prv_bytes     = prv_bytes.left(Type::Identity::KEYSIZE/8/2);
		_object->_prv           = X25519PrivateKey::from_private_bytes(_object->_prv_bytes);
		assert(_object->_prv);
		//TRACEF("Identity::load_private_key: prv bytes:     %s", _object->_prv_bytes.toHex().c_str());

		//p self.sig_prv_bytes = prv_bytes[Identity.KEYSIZE//8//2:]
		_object->_sig_prv_bytes = prv_bytes.mid(Type::Identity::KEYSIZE/8/2);
		_object->_sig_prv       = Ed25519PrivateKey::from_private_bytes(_object->_sig_prv_bytes);
		assert(_object->_sig_prv);
		//TRACEF("Identity::load_private_key: sig prv bytes: %s", _object->_sig_prv_bytes.toHex().c_str());

		_object->_pub           = _object->_prv->public_key();
		assert(_object->_pub);
		_object->_pub_bytes     = _object->_pub->public_bytes();
		//TRACEF("Identity::load_private_key: pub bytes:     %s", _object->_pub_bytes.toHex().c_str());

		_object->_sig_pub       = _object->_sig_prv->public_key();
		assert(_object->_sig_pub);
		_object->_sig_pub_bytes = _object->_sig_pub->public_bytes();
		//TRACEF("Identity::load_private_key: sig pub bytes: %s", _object->_sig_pub_bytes.toHex().c_str());

		update_hashes();

		return true;
	}
	catch (const std::exception& e) {
		//p raise e
		ERROR("Failed to load identity key");
		ERRORF("The contained exception was: %s", e.what());
		return false;
	}
}

/*
Load a public key into the instance.

:param pub_bytes: The public key as *bytes*.
:returns: True if the key was loaded, otherwise False.
*/
void Identity::load_public_key(const Bytes& pub_bytes) {
	assert(_object);

	try {

		//_pub_bytes     = pub_bytes[:Identity.KEYSIZE//8//2]
		_object->_pub_bytes     = pub_bytes.left(Type::Identity::KEYSIZE/8/2);
		//TRACEF("Identity::load_public_key: pub bytes:     ", _object->_pub_bytes.toHex().c_str());

		//_sig_pub_bytes = pub_bytes[Identity.KEYSIZE//8//2:]
		_object->_sig_pub_bytes = pub_bytes.mid(Type::Identity::KEYSIZE/8/2);
		//TRACEF("Identity::load_public_key: sig pub bytes: ", _object->_sig_pub_bytes.toHex().c_str());

		_object->_pub           = X25519PublicKey::from_public_bytes(_object->_pub_bytes);
		_object->_sig_pub       = Ed25519PublicKey::from_public_bytes(_object->_sig_pub_bytes);

		update_hashes();
	}
	catch (const std::exception& e) {
		ERRORF("Error while loading public key, the contained exception was: %s", e.what());
	}
}

bool Identity::load(const char* path) {
	TRACE("Reading identity key from storage...");
#if defined(RNS_USE_FS)
	try {
		Bytes prv_bytes;
		if (OS::read_file(path, prv_bytes) > 0) {
			return load_private_key(prv_bytes);
		}
		else {
			return false;
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error while loading identity from %s", path);
		ERRORF("The contained exception was: %s", e.what());
	}
#endif
	return false;
}

/*
Saves the identity to a file. This will write the private key to disk,
and anyone with access to this file will be able to decrypt all
communication for the identity. Be very careful with this method.

:param path: The full path specifying where to save the identity.
:returns: True if the file was saved, otherwise False.
*/
bool Identity::to_file(const char* path) {
	TRACE("Writing identity key to storage...");
#if defined(RNS_USE_FS)
	try {
		return (OS::write_file(path, get_private_key()) == get_private_key().size());
	}
	catch (const std::exception& e) {
		ERRORF("Error while saving identity to %s", path);
		ERRORF("The contained exception was: %s", e.what());
	}
#endif
	return false;
}


/*
Create a new :ref:`RNS.Identity<api-identity>` instance from a file.
Can be used to load previously created and saved identities into Reticulum.

:param path: The full path to the saved :ref:`RNS.Identity<api-identity>` data
:returns: A :ref:`RNS.Identity<api-identity>` instance, or *None* if the loaded data was invalid.
*/
/*static*/ const Identity Identity::from_file(const char* path) {
	Identity identity(false);
	if (identity.load(path)) {
		return identity;
	}
	return {Type::NONE};
}

/*static*/ void Identity::remember(const Bytes& packet_hash, const Bytes& destination_hash, const Bytes& public_key, const Bytes& app_data /*= {Bytes::NONE}*/) {
	if (public_key.size() != Type::Identity::KEYSIZE/8) {
		throw std::invalid_argument("Can't remember " + destination_hash.toHex() + ", the public key size of " + std::to_string(public_key.size()) + " is not valid.");
	}
	IdentityEntry entry(OS::time(), packet_hash, public_key, app_data);
	if (!_known_destinations.put(destination_hash, entry)) {
		ERRORF("remember: failed to store identity for %s", destination_hash.toHex().c_str());
	}
}

/*
Recall identity for a destination hash.

:param destination_hash: Destination hash as *bytes*.
:returns: An :ref:`RNS.Identity<api-identity>` instance that can be used to create an outgoing :ref:`RNS.Destination<api-destination>`, or *None* if the destination is unknown.
*/
/*static*/ Identity Identity::recall(const Bytes& destination_hash) {
	TRACE("Identity::recall...");

	IdentityEntry identity_data;
	if (_known_destinations.get(destination_hash, identity_data) && identity_data) {
		TRACEF("Identity::recall: Found identity entry for destination %s", destination_hash.toHex().c_str());
		Identity identity(false);
		identity.load_public_key(identity_data._public_key);
		identity.app_data(identity_data._app_data);
		return identity;
	}

#if RNS_IDENTITY_ANNOUNCE_RECALL
	// DIVERGENCE: consult path table for identity recall
	// Since the path table stores the latest announce for each destination (which contains the needed public key etc.),
	// the identity can be restored even if it is not cached in _known_destinations.
	TRACEF("Identity::recall: Unable to find identity entry for destination %s, performing announce lookup...", destination_hash.toHex().c_str());
	Packet announce_packet = Transport::find_announce_packet_from_hash(destination_hash);
	if (announce_packet) {
		TRACEF("Identity::recall: Extracted identity entry from announce packet for destination %s", destination_hash.toHex().c_str());
		Bytes public_key = announce_packet.data().left(KEYSIZE/8);
		Bytes app_data = announce_packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + SIGLENGTH/8);	

		Identity identity(false);
		identity.load_public_key(public_key);
		identity.app_data(app_data);

		remember(announce_packet.get_hash(), destination_hash, public_key, app_data);
		return identity;
	}
#endif

	TRACEF("Identity::recall: Unable to find identity entry for destination %s, performing destination lookup...", destination_hash.toHex().c_str());
	Destination registered_destination(Transport::find_destination_from_hash(destination_hash));
	if (registered_destination) {
		TRACEF("Identity::recall: Found destination %s", destination_hash.toHex().c_str());
		if (!registered_destination.identity()) {
			TRACEF("Identity::recall: Destination %s has no associated identity", destination_hash.toHex().c_str());
			return {Type::NONE};
		}
		Identity identity(false);
		identity.load_public_key(registered_destination.identity().get_public_key());
		identity.app_data({Bytes::NONE});
		return identity;
	}

	TRACEF("Identity::recall: Unable to find destination %s", destination_hash.toHex().c_str());
	return {Type::NONE};
}

/*
Recall last heard app_data for a destination hash.

:param destination_hash: Destination hash as *bytes*.
:returns: *Bytes* containing app_data, or *None* if the destination is unknown.
*/
/*static*/ Bytes Identity::recall_app_data(const Bytes& destination_hash) {
	TRACE("Identity::recall_app_data...");
	IdentityEntry identity_data;
	if (_known_destinations.get(destination_hash, identity_data) && identity_data) {
		TRACEF("Identity::recall_app_data: Found identity entry for destination %s", destination_hash.toHex().c_str());
		return identity_data._app_data;
	}
	TRACEF("Identity::recall_app_data: Unable to find identity entry for destination %s", destination_hash.toHex().c_str());
	return {Bytes::NONE};
}

/*static*/ bool Identity::validate_announce(const Packet& packet, bool only_validate_signature /*= false*/) {
	try {
		if (packet.packet_type() == Type::Packet::ANNOUNCE) {
			Bytes destination_hash = packet.destination_hash();

            // Get public key bytes from announce
			Bytes public_key = packet.data().left(KEYSIZE/8);

			Bytes name_hash;
			Bytes random_hash;
			Bytes ratchet;
			Bytes signature;
			Bytes app_data;

			// If the packet context flag is set,
			// this announce contains a new ratchet
			if (packet.context_flag() == Type::Packet::FLAG_SET) {
				name_hash = packet.data().mid(KEYSIZE/8, NAME_HASH_LENGTH/8);
				random_hash = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8, RANDOM_HASH_LENGTH/8);
				ratchet = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8, RATCHETSIZE/8);
				signature = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + RATCHETSIZE/8, SIGLENGTH/8);
				if (packet.data().size() > (KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + RATCHETSIZE/8 + SIGLENGTH/8)) {
					app_data = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + RATCHETSIZE/8 + SIGLENGTH/8);
				}
			}
			// If the packet context flag is not set,
			// this announce does not contain a ratchet
			else {
				name_hash = packet.data().mid(KEYSIZE/8, NAME_HASH_LENGTH/8);
				random_hash = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8, RANDOM_HASH_LENGTH/8);
				signature = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8, SIGLENGTH/8);
				if (packet.data().size() > (KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + SIGLENGTH/8)) {
					app_data = packet.data().mid(KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + SIGLENGTH/8);
				}
			}

/*
			TRACEF("Identity::validate_announce: destination_hash: %s", destination_hash.toHex().c_str());
			TRACEF("Identity::validate_announce: public_key:       %s", public_key.toHex().c_str());
			TRACEF("Identity::validate_announce: name_hash:        %s", name_hash.toHex().c_str());
			TRACEF("Identity::validate_announce: random_hash:      %s", random_hash.toHex().c_str());
			TRACEF("Identity::validate_announce: ratchet:          %s", ratchet.toHex().c_str());
			TRACEF("Identity::validate_announce: signature:        %s", signature.toHex().c_str());
			TRACEF("Identity::validate_announce: app_data:         %s", app_data.toHex().c_str());
			TRACEF("Identity::validate_announce: app_data text:    %s", app_data.toString().c_str());
*/

			Bytes signed_data;
			signed_data << destination_hash << public_key << name_hash << random_hash << ratchet << app_data;
			//TRACEF("Identity::validate_announce: signed_data:      %s", signed_data.toHex().c_str());

			if (packet.data().size() <= KEYSIZE/8 + NAME_HASH_LENGTH/8 + RANDOM_HASH_LENGTH/8 + SIGLENGTH/8) {
				app_data.clear();
			}

			Identity announced_identity(false);
			announced_identity.load_public_key(public_key);

			if (announced_identity.pub() && announced_identity.validate(signature, signed_data)) {
				if (only_validate_signature) {
					//p del announced_identity
					return true;
				}

				Bytes hash_material = name_hash << announced_identity.hash();
				Bytes expected_hash = full_hash(hash_material).left(Type::Reticulum::TRUNCATED_HASHLENGTH/8);
				//TRACEF("Identity::validate_announce: destination_hash: %s", destination_hash.toHex().c_str());
				//TRACEF("Identity::validate_announce: expected_hash:    %s", expected_hash.toHex().c_str());

				if (destination_hash == expected_hash) {
					// Check if we already have a public key for this destination
					// and make sure the public key is not different.
					IdentityEntry identity_entry;
					if (_known_destinations.get(destination_hash, identity_entry) && identity_entry) {
						if (public_key != identity_entry._public_key) {
							// In reality, this should never occur, but in the odd case
							// that someone manages a hash collision, we reject the announce.
							CRITICAL("Received announce with valid signature and destination hash, but announced public key does not match already known public key.");
							CRITICAL("This may indicate an attempt to modify network paths, or a random hash collision. The announce was rejected.");
							return false;
						}
					}

					remember(packet.get_hash(), destination_hash, public_key, app_data);
					//p del announced_identity

					std::string signal_str;
// TODO
/*
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
*/

					if (packet.transport_id()) {
						TRACEF("Valid announce for %s %d hops away, received via %s on %s%s", destination_hash.toHex().c_str(), packet.hops(), packet.transport_id().toHex().c_str(), packet.receiving_interface().toString().c_str(), signal_str.c_str());
					}
					else {
						TRACEF("Valid announce for %s %d hops away, received on %s%s", destination_hash.toHex().c_str(), packet.hops(), packet.receiving_interface().toString().c_str(), signal_str.c_str());
					}

					return true;
				}
				else {
					DEBUGF("Received invalid announce for %s: Destination mismatch.", destination_hash.toHex().c_str());
					return false;
				}
			}
			else {
				DEBUGF("Received invalid announce for %s: Invalid signature.", destination_hash.toHex().c_str());
				//p del announced_identity
				return false;
			}
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error occurred while validating announce. The contained exception was: %s", e.what());
		return false;
	}
	return false;
}

/*
Encrypts information for the identity.

:param plaintext: The plaintext to be encrypted as *bytes*.
:returns: Ciphertext token as *bytes*.
:raises: *KeyError* if the instance does not hold a public key.
*/
const Bytes Identity::encrypt(const Bytes& plaintext) const {
	assert(_object);
	TRACE("Identity::encrypt: encrypting data...");
	if (!_object->_pub) {
		throw std::runtime_error("Encryption failed because identity does not hold a public key");
	}
	Cryptography::X25519PrivateKey::Ptr ephemeral_key = Cryptography::X25519PrivateKey::generate();
	Bytes ephemeral_pub_bytes = ephemeral_key->public_key()->public_bytes();
	TRACEF("Identity::encrypt: ephemeral public key: %s", ephemeral_pub_bytes.toHex().c_str());

	// CRYPTO: create shared key for key exchange using own public key
	//shared_key = ephemeral_key.exchange(self.pub)
	Bytes shared_key = ephemeral_key->exchange(_object->_pub_bytes);
	TRACEF("Identity::encrypt: shared key:           %s", shared_key.toHex().c_str());

	Bytes derived_key = Cryptography::hkdf(
		DERIVED_KEY_LENGTH,
		shared_key,
		get_salt(),
		get_context()
	);
	TRACEF("Identity::encrypt: derived key:          %s", derived_key.toHex().c_str());

	Cryptography::Token token(derived_key);
	TRACEF("Identity::encrypt: Token encrypting data of length %lu", plaintext.size());
	TRACEF("Identity::encrypt: plaintext:  %s", plaintext.toHex().c_str());
	Bytes ciphertext = token.encrypt(plaintext);
	TRACEF("Identity::encrypt: ciphertext: %s", ciphertext.toHex().c_str());

	return ephemeral_pub_bytes + ciphertext;
}


/*
Decrypts information for the identity.

:param ciphertext: The ciphertext to be decrypted as *bytes*.
:returns: Plaintext as *bytes*, or *None* if decryption fails.
:raises: *KeyError* if the instance does not hold a private key.
*/
const Bytes Identity::decrypt(const Bytes& ciphertext_token) const {
	assert(_object);
	TRACE("Identity::decrypt: decrypting data...");
	if (!_object->_prv) {
		throw std::runtime_error("Decryption failed because identity does not hold a private key");
	}
	if (ciphertext_token.size() <= Type::Identity::KEYSIZE/8/2) {
		DEBUGF("Decryption failed because the token size %lu was invalid.", ciphertext_token.size());
		return {Bytes::NONE};
	}
	Bytes plaintext;
	try {
		//peer_pub_bytes = ciphertext_token[:Identity.KEYSIZE//8//2]
		Bytes peer_pub_bytes = ciphertext_token.left(Type::Identity::KEYSIZE/8/2);
		//peer_pub = X25519PublicKey.from_public_bytes(peer_pub_bytes)
		//Cryptography::X25519PublicKey::Ptr peer_pub = Cryptography::X25519PublicKey::from_public_bytes(peer_pub_bytes);
		TRACEF("Identity::decrypt: peer public key:      %s", peer_pub_bytes.toHex().c_str());


		// CRYPTO: create shared key for key exchange using peer public key
		//shared_key = _object->_prv->exchange(peer_pub);
		Bytes shared_key = _object->_prv->exchange(peer_pub_bytes);
		TRACEF("Identity::decrypt: shared key:           %s", shared_key.toHex().c_str());

		Bytes derived_key = Cryptography::hkdf(
			DERIVED_KEY_LENGTH,
			shared_key,
			get_salt(),
			get_context()
		);
		TRACEF("Identity::decrypt: derived key:          %s", derived_key.toHex().c_str());

		Cryptography::Token token(derived_key);
		//ciphertext = ciphertext_token[Identity.KEYSIZE//8//2:]
		Bytes ciphertext(ciphertext_token.mid(Type::Identity::KEYSIZE/8/2));
		TRACEF("Identity::decrypt: Token decrypting data of length %lu", ciphertext.size());
		TRACEF("Identity::decrypt: ciphertext: %s", ciphertext.toHex().c_str());
		plaintext = token.decrypt(ciphertext);
		TRACEF("Identity::decrypt: plaintext:  %s", plaintext.toHex().c_str());
		//TRACEF("Identity::decrypt: Token decrypted data of length %lu", plaintext.size());
	}
	catch (const std::exception& e) {
		DEBUGF("Decryption by %s failed: %s", toString().c_str(), e.what());
	}
		
	return plaintext;
}

/*
Signs information by the identity.

:param message: The message to be signed as *bytes*.
:returns: Signature as *bytes*.
:raises: *KeyError* if the instance does not hold a private key.
*/
const Bytes Identity::sign(const Bytes& message) const {
	assert(_object);
	if (!_object->_sig_prv) {
		throw std::runtime_error("Signing failed because identity does not hold a private key");
	}
	try {
		return _object->_sig_prv->sign(message);
	}
	catch (const std::exception& e) {
		ERRORF("The identity %s could not sign the requested message. The contained exception was: %s", toString().c_str(), e.what());
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
bool Identity::validate(const Bytes& signature, const Bytes& message) const {
	assert(_object);
	if (_object->_pub) {
		try {
			TRACEF("Identity::validate: Verifying signature: %s against message: %s", signature.toHex().c_str(), message.toHex().c_str());
			return _object->_sig_pub->verify(signature, message);
		}
		catch (const std::exception& e) {
			return false;
		}
	}
	else {
		throw std::runtime_error("Signature validation failed because identity does not hold a public key");
	}
}

void Identity::prove(const Packet& packet, const Destination& destination /*= {Type::NONE}*/) const {
	assert(_object);
	Bytes signature(sign(packet.packet_hash()));
	Bytes proof_data;
	if (RNS::Reticulum::should_use_implicit_proof()) {
		proof_data = signature;
		TRACEF("Identity::prove: implicit proof data: %s", proof_data.toHex().c_str());
	}
	else {
		proof_data = packet.packet_hash() + signature;
		TRACEF("Identity::prove: explicit proof data: %s", proof_data.toHex().c_str());
	}
	
	if (!destination) {
		TRACE("Identity::prove: proving packet with proof destination...");
		ProofDestination proof_destination = packet.generate_proof_destination();
		Packet(proof_destination, proof_data)
			.attached_interface(packet.receiving_interface())
			.packet_type(Type::Packet::PROOF)
			.send();
	}
	else {
		TRACE("Identity::prove: proving packet with specified destination...");
		Packet(destination, proof_data)
			.attached_interface(packet.receiving_interface())
			.packet_type(Type::Packet::PROOF)
			.send();
	}
}

void Identity::prove(const Packet& packet) const {
	prove(packet, {Type::NONE});
}
