#include "Destination.h"

#include "Log.h"
#include "Transport.h"
#include "Packet.h"
#include "Interfaces/Interface.h"

#include <time.h>
#include <string.h>
#include <stdexcept>
#include <vector>

using namespace RNS;

Destination::Destination(const Identity &identity, const directions direction, const types type, const char* app_name, const char *aspects) : _object(new Object(identity)) {
	assert(_object);

	// Check input values and build name string
	if (strchr(app_name, '.') != nullptr) {
		throw std::invalid_argument("Dots can't be used in app names");
	}

	_object->_type = type;
	_object->_direction = direction;

	std::string fullaspects(aspects);
	if (!identity && direction == Destination::IN && _object->_type != Destination::PLAIN) {
		debug("Destination::Destination: identity not provided, creating new one");
		_object->_identity = Identity();
		// CBA TODO should following include a "." delimiter?
		fullaspects += _object->_identity.hexhash();
	}
	debug("Destination::Destination: full aspects: " + fullaspects);

	if (_object->_identity && _object->_type == Destination::PLAIN) {
		throw std::invalid_argument("Selected destination type PLAIN cannot hold an identity");
	}

	_object->_name = expand_name(_object->_identity, app_name, fullaspects.c_str());
	debug("Destination::Destination: name: " + _object->_name);

	// Generate the destination address hash
	debug("Destination::Destination: creating hash...");
	_object->_hash = hash(_object->_identity, app_name, fullaspects.c_str());
	debug("Destination::Destination: hash: " + _object->_hash.toHex());
	// CBA TEST CRASH
	debug("Destination::Destination: creating name hash...");
	_object->_name_hash = Identity::truncated_hash(expand_name(Identity::NONE, app_name, fullaspects.c_str()));
	debug("Destination::Destination: name hash: " + _object->_name_hash.toHex());
	_object->_hexhash = _object->_hash.toHex();
	debug("Destination::Destination: hexhash: " + _object->_hexhash);

	debug("Destination::Destination: calling register_destination");
	Transport::register_destination(*this);

	extreme("Destination object created");
}

Destination::~Destination() {
    extreme("Destination object destroyed");
}

/*
:returns: A destination name in adressable hash form, for an app_name and a number of aspects.
*/
/*static*/ Bytes Destination::hash(const Identity &identity, const char *app_name, const char *aspects) {
	//name_hash = Identity::full_hash(Destination.expand_name(None, app_name, *aspects).encode("utf-8"))[:(RNS.Identity.NAME_HASH_LENGTH//8)]
	//addr_hash_material = name_hash
	Bytes addr_hash_material = Identity::truncated_hash(expand_name(Identity::NONE, app_name, aspects));
	//if identity != None:
	//	if isinstance(identity, RNS.Identity):
	//		addr_hash_material += identity.hash
	//	elif isinstance(identity, bytes) and len(identity) == RNS.Reticulum.TRUNCATED_HASHLENGTH//8:
	//		addr_hash_material += identity
	//	else:
	//		raise TypeError("Invalid material supplied for destination hash calculation")
	addr_hash_material << identity.hash();

	return Identity::truncated_hash(addr_hash_material);
}

/*
:returns: A string containing the full human-readable name of the destination, for an app_name and a number of aspects.
*/
/*static*/ std::string Destination::expand_name(const Identity &identity, const char *app_name, const char *aspects) {

	if (strchr(app_name, '.') != nullptr) {
		throw std::invalid_argument("Dots can't be used in app names");
	}

	std::string name(app_name);

	if (aspects != nullptr) {
		name += std::string(".") + aspects;
	}

	if (identity) {
		name += "." + identity.hexhash();
	}

	return name;
}

/*
Creates an announce packet for this destination and broadcasts it on all
relevant interfaces. Application specific data can be added to the announce.

:param app_data: *bytes* containing the app_data.
:param path_response: Internal flag used by :ref:`RNS.Transport<api-transport>`. Ignore.
*/
Packet Destination::announce(const Bytes &app_data, bool path_response, Interface *attached_interface, const Bytes &tag, bool send) {
	assert(_object);
	debug("Destination::announce: announcing destination...");

	if (_object->_type != SINGLE) {
		throw std::invalid_argument("Only SINGLE destination types can be announced");
	}

	if (_object->_direction != IN) {
		throw std::invalid_argument("Only IN destination types can be announced");
	}

	time_t now = time(nullptr);
    auto it = _object->_path_responses.begin();
    while (it != _object->_path_responses.end()) {
		// vector
		//Response &entry = *it;
		// map
		Response &entry = (*it).second;
		if (now > (entry.first + Destination::PR_TAG_WINDOW)) {
			it = _object->_path_responses.erase(it);
		}
		else {
			++it;
		}
	}

	Bytes announce_data;

/*
	// CBA TEST
	debug("Destination::announce: performing path test...");
	debug("Destination::announce: inserting path...");
	_object->_path_responses.insert({Bytes("foo_tag"), {0, Bytes("this is foo tag")}});
	debug("Destination::announce: inserting path...");
	_object->_path_responses.insert({Bytes("test_tag"), {0, Bytes("this is test tag")}});
	if (path_response) {
		debug("Destination::announce: path_response is true");
	}
	if (!tag.empty()) {
		debug("Destination::announce: tag is specified");
		std::string tagstr((const char*)tag.data(), tag.size());
		debug(std::string("Destination::announce: tag: ") + tagstr);
		debug(std::string("Destination::announce: tag len: ") + std::to_string(tag.size()));
		debug("Destination::announce: searching for tag...");
		if (_object->_path_responses.find(tag) != _object->_path_responses.end()) {
			debug("Destination::announce: found tag in _path_responses");
			debug(std::string("Destination::announce: data: ") +_object->_path_responses[tag].second.toString());
		}
		else {
			debug("Destination::announce: tag not found in _path_responses");
		}
	}
	debug("Destination::announce: path test finished");
*/

	if (path_response && !tag.empty() && _object->_path_responses.find(tag) != _object->_path_responses.end()) {
		// This code is currently not used, since Transport will block duplicate
		// path requests based on tags. When multi-path support is implemented in
		// Transport, this will allow Transport to detect redundant paths to the
		// same destination, and select the best one based on chosen criteria,
		// since it will be able to detect that a single emitted announce was
		// received via multiple paths. The difference in reception time will
		// potentially also be useful in determining characteristics of the
		// multiple available paths, and to choose the best one.
		//zextreme("Using cached announce data for answering path request with tag "+RNS.prettyhexrep(tag));
		announce_data << _object->_path_responses[tag].second;
	}
	else {
		Bytes destination_hash = _object->_hash;
		//zBytes random_hash = Identity::get_random_hash()[0:5] << int(time.time()).to_bytes(5, "big");
		Bytes random_hash;

		Bytes new_app_data(app_data);
        if (new_app_data.empty() && !_object->_default_app_data.empty()) {
			new_app_data = _object->_default_app_data;
		}

		Bytes signed_data;
		debug("Destination::announce: hash: " + _object->_hash.toHex());
		debug("Destination::announce: identity public key: " + _object->_identity.get_public_key().toHex());
		debug("Destination::announce: name hash: " + _object->_name_hash.toHex());
		debug("Destination::announce: random hash: " + random_hash.toHex());
		debug("Destination::announce: new app data: " + new_app_data.toHex());
		signed_data << _object->_hash << _object->_identity.get_public_key() << _object->_name_hash << random_hash;
		if (new_app_data) {
			signed_data << new_app_data;
		}
		debug("Destination::announce: signed data: " + signed_data.toHex());

		Bytes signature(_object->_identity.sign(signed_data));
		debug("Destination::announce: signature: " + signature.toHex());

		announce_data << _object->_identity.get_public_key() << _object->_name_hash << random_hash << signature;

		if (new_app_data) {
			announce_data << new_app_data;
		}

		_object->_path_responses.insert({tag, {time(nullptr), announce_data}});
	}
	debug("Destination::announce: announce_data: " + announce_data.toHex());

	Packet::context_types announce_context = Packet::CONTEXT_NONE;
	if (path_response) {
		announce_context = Packet::PATH_RESPONSE;
	}

	debug("Destination::announce: creating announce packet...");
	Packet announce_packet(*this, announce_data, Packet::ANNOUNCE, announce_context,Transport::BROADCAST, Packet::HEADER_1, nullptr, attached_interface);

	if (send) {
		announce_packet.send();
		return Packet::NONE;
	}
	else {
		return announce_packet;
	}
}

/*
	Registers a function to be called when a link has been established to
	this destination.

	:param callback: A function or method with the signature *callback(link)* to be called when a new link is established with this destination.
*/
void Destination::set_link_established_callback(Callbacks::link_established callback) {
	assert(_object);
	_object->_callbacks._link_established = callback;
}

/*
	Registers a function to be called when a packet has been received by
	this destination.

	:param callback: A function or method with the signature *callback(data, packet)* to be called when this destination receives a packet.
*/
void Destination::set_packet_callback(Callbacks::packet callback) {
	assert(_object);
	_object->_callbacks._packet = callback;
}

/*
	Registers a function to be called when a proof has been requested for
	a packet sent to this destination. Allows control over when and if
	proofs should be returned for received packets.

	:param callback: A function or method to with the signature *callback(packet)* be called when a packet that requests a proof is received. The callback must return one of True or False. If the callback returns True, a proof will be sent. If it returns False, a proof will not be sent.
*/
void Destination::set_proof_requested_callback(Callbacks::proof_requested callback) {
	assert(_object);
	_object->_callbacks._proof_requested = callback;
}

/*
Encrypts information for ``RNS.Destination.SINGLE`` or ``RNS.Destination.GROUP`` type destination.

:param plaintext: A *bytes-like* containing the plaintext to be encrypted.
:raises: ``ValueError`` if destination does not hold a necessary key for encryption.
*/
Bytes Destination::encrypt(const Bytes &data) {
	assert(_object);
	debug("Destination::encrypt: encrypting bytes");

	if (_object->_type == Destination::PLAIN) {
		return data;
	}

	if (_object->_type == Destination::SINGLE && _object->_identity) {
		return _object->_identity.encrypt(data);
	}

/*
	if (_object->_type == Destination::GROUP {
		if hasattr(self, "prv") and self.prv != None:
			try:
				return self.prv.encrypt(plaintext)
			except Exception as e:
				RNS.log("The GROUP destination could not encrypt data", RNS.LOG_ERROR)
				RNS.log("The contained exception was: "+str(e), RNS.LOG_ERROR)
		else:
			raise ValueError("No private key held by GROUP destination. Did you create or load one?")
	}
*/
	// MOCK
	return Bytes::NONE;
}

/*
Decrypts information for ``RNS.Destination.SINGLE`` or ``RNS.Destination.GROUP`` type destination.

:param ciphertext: *Bytes* containing the ciphertext to be decrypted.
:raises: ``ValueError`` if destination does not hold a necessary key for decryption.
*/
Bytes Destination::decrypt(const Bytes &data) {
	assert(_object);
	debug("Destination::decrypt: decrypting bytes");

	if (_object->_type == Destination::PLAIN) {
		return data;
	}

	if (_object->_type == Destination::SINGLE && _object->_identity) {
		return _object->_identity.decrypt(data);
	}

/*
	if (_object->_type == Destination::GROUP) {
		if hasattr(self, "prv") and self.prv != None:
			try:
				return self.prv.decrypt(ciphertext)
			except Exception as e:
				RNS.log("The GROUP destination could not decrypt data", RNS.LOG_ERROR)
				RNS.log("The contained exception was: "+str(e), RNS.LOG_ERROR)
		else:
			raise ValueError("No private key held by GROUP destination. Did you create or load one?")
	}
*/
	// MOCK
	return Bytes::NONE;
}

/*
Signs information for ``RNS.Destination.SINGLE`` type destination.

:param message: *Bytes* containing the message to be signed.
:returns: A *bytes-like* containing the message signature, or *None* if the destination could not sign the message.
*/
Bytes Destination::sign(const Bytes &message) {
	assert(_object);
	if (_object->_type == Destination::SINGLE && _object->_identity) {
		return _object->_identity.sign(message);
	}
	return Bytes::NONE;
}
