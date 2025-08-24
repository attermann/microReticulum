#include "Link.h"

#include "LinkData.h"
#include "Resource.h"
#include "Reticulum.h"
#include "Transport.h"
#include "Packet.h"
#include "Log.h"
#include "Cryptography/Ed25519.h"
#include "Cryptography/X25519.h"
#include "Cryptography/HKDF.h"
#include "Cryptography/Token.h"
#include "Cryptography/Random.h"

#define MSGPACK_DEBUGLOG_ENABLE 0
#include <MsgPack.h>

#include <algorithm>

using namespace RNS;
using namespace RNS::Type::Link;
using namespace RNS::Cryptography;
using namespace RNS::Utilities;

/*static*/ uint8_t Link::resource_strategies = ACCEPT_NONE | ACCEPT_APP | ACCEPT_ALL;

/*static*/ std::set<link_mode> Link::ENABLED_MODES = {MODE_AES256_CBC};
/*static*/ link_mode Link::MODE_DEFAULT = MODE_AES256_CBC;

Link::Link(const Destination& destination /*= {Type::NONE}*/, Callbacks::established established_callback /*= nullptr*/, Callbacks::closed closed_callback /*= nullptr*/, const Destination& owner /*= {Type::NONE}*/, const Bytes& peer_pub_bytes /*= {Bytes::NONE}*/, const Bytes& peer_sig_pub_bytes /*= {Bytes::NONE}*/, link_mode mode /*= MODE_DEFAULT*/) :
	_object(new LinkData(destination))
{
	assert(_object);

	_object->_owner = owner;
	_object->_mode = mode;

	if (destination && destination.type() != Type::Destination::SINGLE) {
		throw std::logic_error("Links can only be established to the \"single\" destination type");
	}

	if (!destination) {
		_object->_initiator = false;
		_object->_prv     = Cryptography::X25519PrivateKey::generate();
		// CBA BUG: not checking for owner
		if (_object->_owner) {
			_object->_sig_prv = _object->_owner.identity().sig_prv();
		}
	}
	else {
		_object->_initiator = true;
		_object->_expected_hops = Transport::hops_to(_object->_destination.hash());
		_object->_establishment_timeout = Reticulum::get_instance().get_first_hop_timeout(destination.hash());
		_object->_prv     = Cryptography::X25519PrivateKey::generate();
		_object->_sig_prv = Cryptography::Ed25519PrivateKey::generate();
	}

	_object->_pub           = _object->_prv->public_key();
	_object->_pub_bytes     = _object->_pub->public_bytes();
	TRACEF("Link::load_private_key: pub bytes:     %s", _object->_pub_bytes.toHex().c_str());

	_object->_sig_pub       = _object->_sig_prv->public_key();
	_object->_sig_pub_bytes = _object->_sig_pub->public_bytes();
	TRACEF("Link::load_private_key: sig pub bytes: %s", _object->_sig_pub_bytes.toHex().c_str());

	if (!peer_pub_bytes) {
		_object->_peer_pub = nullptr;
		_object->_peer_pub_bytes = nullptr;
	}
	else {
		load_peer(peer_pub_bytes, peer_sig_pub_bytes);
	}

	if (established_callback) {
		set_link_established_callback(established_callback);
	}

	if (closed_callback) {
		set_link_closed_callback(closed_callback);
	}

	if (_object->_initiator) {
		Bytes signalling_bytes;
		uint16_t nh_hw_mtu = Transport::next_hop_interface_hw_mtu(destination.hash());
		if (RNS::Reticulum::link_mtu_discovery() && nh_hw_mtu) {
			signalling_bytes = Link::signalling_bytes(nh_hw_mtu, _object->_mode);
			DEBUGF("Signalling link MTU of %d for link", nh_hw_mtu);
		}
		else {
			signalling_bytes = Link::signalling_bytes(RNS::Type::Reticulum::MTU, _object->_mode);
		}
		TRACEF("Establishing link with mode %d", _object->_mode);
        //p self.request_data = self.pub_bytes+self.sig_pub_bytes+signalling_bytes
		_object->_request_data = _object->_pub_bytes + _object->_sig_pub_bytes + signalling_bytes;
		_object->_packet = Packet(destination, _object->_request_data, RNS::Type::Packet::LINKREQUEST);
		_object->_packet.pack();
		_object->_establishment_cost += _object->_packet.raw().size();
		set_link_id(_object->_packet);
		Transport::register_link(*this);
		_object->_request_time = OS::time();
		start_watchdog();
		_object->_packet.send();
		had_outbound();
		DEBUGF("Link request %s sent to %s", _object->_link_id.toHex().c_str(), _object->_destination.toString().c_str());
		TRACEF("Establishment timeout is %f for link request %s", _object->_establishment_timeout, _object->_link_id.toHex().c_str());
	}

	// CBA LINK
	//_object->_link_destination = Destination({Type::NONE}, Type::Destination::OUT, Type::Destination::LINK, hash());
	//_object->_link_destination.link_id(_object->_link_id);

	MEM("Link object created");
}


/*static*/ Bytes Link::signalling_bytes(uint16_t mtu, link_mode mode) {
	//p if not mode in Link.ENABLED_MODES: raise TypeError(f"Requested link mode {Link.MODE_DESCRIPTIONS[mode]} not enabled")
	//if (!(mode & ENABLED_MODES)) throw std::runtime_error("Requested link mode "+std::to_string(mode)+" not enabled");
	if (Link::ENABLED_MODES.find(mode) == Link::ENABLED_MODES.end()) throw std::runtime_error("Requested link mode "+std::to_string(mode)+" not enabled");
	//p signalling_value = (mtu & Link.MTU_BYTEMASK)+(((mode<<5) & Link.MODE_BYTEMASK)<<16)
	//p return struct.pack(">I", signalling_value)[1:]
	uint32_t signalling_value = htonl((mtu & MTU_BYTEMASK)+(((mode<<5) & MODE_BYTEMASK)<<16));
	Bytes data(((uint8_t*)&signalling_value)+1, 3);
	return data;
}

/*static*/ uint16_t Link::mtu_from_lr_packet(const Packet& packet) {
	if (packet.data().size() == ECPUBSIZE+LINK_MTU_SIZE) {
		return (packet.data()[ECPUBSIZE] << 16) + (packet.data()[ECPUBSIZE+1] << 8) + (packet.data()[ECPUBSIZE+2]) & MTU_BYTEMASK;
	}
	return 0;
}

/*static*/ uint16_t Link::mtu_from_lp_packet(const Packet& packet) {
	//p if len(packet.data) == RNS.Identity.SIGLENGTH//8+Link.ECPUBSIZE//2+Link.LINK_MTU_SIZE:
	if (packet.data().size() == RNS::Type::Identity::SIGLENGTH/8+ECPUBSIZE/2+LINK_MTU_SIZE) {
		Bytes mtu_bytes = packet.data().mid(RNS::Type::Identity::SIGLENGTH/8+ECPUBSIZE/2, LINK_MTU_SIZE);
		return (mtu_bytes[0] << 16) + (mtu_bytes[1] << 8) + (mtu_bytes[2]) & MTU_BYTEMASK;
	}
	return 0;
}

/*static*/ uint8_t Link::mode_byte(link_mode mode) {
	//p if mode in Link.ENABLED_MODES: return (mode << 5) & Link.MODE_BYTEMASK
	if (Link::ENABLED_MODES.find(mode) != Link::ENABLED_MODES.end()) return (mode << 5) & MODE_BYTEMASK;
	throw std::runtime_error("Requested link mode {mode} not enabled");
}

/*static*/ link_mode Link::mode_from_lr_packet(const Packet& packet) {
	if (packet.data().size() > ECPUBSIZE) {
		link_mode mode = static_cast<link_mode>((packet.data()[ECPUBSIZE] & MODE_BYTEMASK) >> 5);
		return mode;
	}
	return Link::MODE_DEFAULT;
}

/*static*/ link_mode Link::mode_from_lp_packet(const Packet& packet) {
	if (packet.data().size() > RNS::Type::Identity::SIGLENGTH/8+ECPUBSIZE/2) {
		link_mode mode = static_cast<link_mode>(packet.data()[RNS::Type::Identity::SIGLENGTH/8+ECPUBSIZE/2] >> 5);
		return mode;
	}
	return Link::MODE_DEFAULT;
}

/*static*/ Bytes Link::link_id_from_lr_packet(const Packet& packet) {
	Bytes hashable_part = packet.get_hashable_part();
	if (packet.data().size() > ECPUBSIZE) {
		size_t diff = packet.data().size() - ECPUBSIZE;
		//p hashable_part = hashable_part[:-diff]
		hashable_part = hashable_part.left(hashable_part.size() - diff);
	}
	return RNS::Identity::truncated_hash(hashable_part);
}

/*static*/ Link Link::validate_request( const Destination& owner, const Bytes& data, const Packet& packet) {
	if (data.size() == ECPUBSIZE) {
		try {
			Link link({Type::NONE}, nullptr, nullptr, owner, data.left(ECPUBSIZE/2), data.mid(ECPUBSIZE/2, ECPUBSIZE/2));
			link.set_link_id(packet);
			link.destination(packet.destination());
			link.establishment_timeout(ESTABLISHMENT_TIMEOUT_PER_HOP * std::max((uint8_t)1, packet.hops()) + KEEPALIVE);
			link.establishment_cost(link.establishment_cost() + packet.raw().size());
			VERBOSEF("Validating link request %s", link.link_id().toHex().c_str());
			TRACEF("Establishment timeout is %f for incoming link request %s", link.establishment_timeout(), link.link_id().toHex().c_str());
			link.handshake();
			link.attached_interface(packet.receiving_interface());
			link.prove();
			link.request_time(OS::time());
			Transport::register_link(link);
			link.last_inbound(OS::time());
			link.start_watchdog();
			
			DEBUGF("Incoming link request %s accepted", link.toString().c_str());
			return link;
		}
		catch (std::exception& e) {
			VERBOSEF("Validating link request failed, exception: %s", e.what());
			return {Type::NONE};
		}
	}
	else {
		DEBUG("Invalid link request payload size, dropping request");
		return {Type::NONE};
	}
}

void Link::load_peer(const Bytes& peer_pub_bytes, const Bytes& peer_sig_pub_bytes) {
	assert(_object);
	_object->_peer_pub_bytes = peer_pub_bytes;
	_object->_peer_pub = Cryptography::X25519PublicKey::from_public_bytes(_object->_peer_pub_bytes);

	_object->_peer_sig_pub_bytes = peer_sig_pub_bytes;
	_object->_peer_sig_pub = Cryptography::Ed25519PublicKey::from_public_bytes(_object->_peer_sig_pub_bytes);

	// CBA TODO Determine the purpose of the following.
	// X25519PublicKey does not have a member "curve" and this is not accessed anywhere
/*
	if not hasattr(self.peer_pub, "curve") {
		self.peer_pub.curve = CURVE;
	}
*/
}

void Link::set_link_id(const Packet& packet) {
	assert(_object);
	_object->_link_id = Link::link_id_from_lr_packet(packet);
	_object->_hash = _object->_link_id;
}

void Link::handshake() {
	assert(_object);
	if (_object->_status == Type::Link::PENDING && _object->_prv) {
		_object->_status = Type::Link::HANDSHAKE;
		_object->_shared_key = _object->_prv->exchange(_object->_peer_pub_bytes);

		uint16_t derived_key_length;
		if (_object->_mode == RNS::Type::Link::MODE_AES128_CBC) derived_key_length = 32;
		else if (_object->_mode == RNS::Type::Link::MODE_AES256_CBC) derived_key_length = 64;
		else throw new std::invalid_argument("Invalid link mode "+std::to_string(_object->_mode)+" on "+toString());

		_object->_derived_key = Cryptography::hkdf(
			derived_key_length,
			_object->_shared_key,
			get_salt(),
			get_context()
		);
	}
	else {
		ERRORF("Handshake attempt on %s with invalid state %d", toString().c_str(), _object->_status);
	}
}

void Link::prove() {
	assert(_object);
	DEBUGF("Link %s requesting proof", link_id().toHex().c_str());
	Bytes signed_data =_object->_link_id + _object->_pub_bytes + _object->_sig_pub_bytes;
	const Bytes signature(_object->_owner.identity().sign(signed_data));

	Bytes proof_data = signature + _object->_pub_bytes;
	// CBA LINK
	// CBA TODO: Determine which approach is better, passing liunk to packet or passing _link_destination
	Packet proof(*this, proof_data, Type::Packet::PROOF, Type::Packet::LRPROOF);
	proof.send();
	_object->_establishment_cost += proof.raw().size();
	had_outbound();
}

void Link::prove_packet(const Packet& packet) {
	assert(_object);
	DEBUGF("Link %s proving packet", link_id().toHex().c_str());
	const Bytes signature(sign(packet.packet_hash()));
	// TODO: Hardcoded as explicit proof for now
	// if RNS.Reticulum.should_use_implicit_proof():
	//   proof_data = signature
	// else:
	//   proof_data = packet.packet_hash + signature
	Bytes proof_data = packet.packet_hash() + signature;

	Packet proof(*this, proof_data, Type::Packet::PROOF);
	proof.send();
	had_outbound();
}

void Link::validate_proof(const Packet& packet) {
	assert(_object);
	DEBUGF("Link %s validating proof", link_id().toHex().c_str());
	try {
		if (_object->_status == Type::Link::PENDING) {
			Bytes packet_data(packet.data());
			TRACEF("Link %s: initiator: %d", toString().c_str(), _object->_initiator);
			TRACEF("Link %s: size: %d", toString().c_str(), packet_data.size());
			Bytes signalling_bytes;
			uint16_t confirmed_mtu = 0;
			link_mode mode = mode_from_lp_packet(packet);
			DEBUGF("Validating link request proof with mode %d", mode);
			if (mode != _object->_mode) throw std::runtime_error("Invalid link mode "+std::to_string(mode)+" in link request proof");
            //p if len(packet.data) == RNS.Identity.SIGLENGTH//8+Link.ECPUBSIZE//2+Link.LINK_MTU_SIZE:
			if (packet_data.size() == Type::Identity::SIGLENGTH/8+ECPUBSIZE/2+RNS::Type::Link::LINK_MTU_SIZE) {
				confirmed_mtu = Link::mtu_from_lp_packet(packet);
				signalling_bytes = Link::signalling_bytes(confirmed_mtu, mode);
				// CBA TODO Determine best way to deal with packet.data() being read-only
				//p packet.data = packet.data[:RNS.Identity.SIGLENGTH//8+Link.ECPUBSIZE//2]
				packet_data = packet_data.left(RNS::Type::Identity::SIGLENGTH/8+ECPUBSIZE/2);
				DEBUGF("Destination confirmed link MTU of %d", confirmed_mtu);
			}
			//p if _object->_initiator and len(packet.data) == RNS.Identity.SIGLENGTH//8+ECPUBSIZE//2:
			if (_object->_initiator && packet_data.size() == Type::Identity::SIGLENGTH/8+ECPUBSIZE/2) {
				//p peer_pub_bytes = packet.data[RNS.Identity.SIGLENGTH//8:RNS.Identity.SIGLENGTH//8+ECPUBSIZE//2]
				const Bytes peer_pub_bytes(packet_data.mid(Type::Identity::SIGLENGTH/8, ECPUBSIZE/2));
				//p peer_sig_pub_bytes = _object->_destination.identity.get_public_key()[ECPUBSIZE//2:ECPUBSIZE]
				const Bytes peer_sig_pub_bytes(_object->_destination.identity().get_public_key().mid(ECPUBSIZE/2, ECPUBSIZE/2));
				TRACEF("Link %s performing handshake", link_id().toHex().c_str());
				load_peer(peer_pub_bytes, peer_sig_pub_bytes);
				handshake();

				_object->_establishment_cost += packet.raw().size();
				Bytes signed_data = _object->_link_id + _object->_peer_pub_bytes + _object->_peer_sig_pub_bytes;
				const Bytes signature(packet_data.left(Type::Identity::SIGLENGTH/8));
				
				TRACEF("Link %s validating identity", link_id().toHex().c_str());
				if (_object->_destination.identity().validate(signature, signed_data)) {
					if (_object->_status != Type::Link::HANDSHAKE) {
						throw std::runtime_error("Invalid link state for proof validation: " + _object->_status);
					}
					_object->_rtt = OS::time() - _object->_request_time;
					_object->_attached_interface = packet.receiving_interface();
					_object->__remote_identity = _object->_destination.identity();
					if (confirmed_mtu) _object->_mtu = confirmed_mtu;
					else _object->_mtu = RNS::Type::Reticulum::MTU;
					update_mdu();
					_object->_status = Type::Link::ACTIVE;
					_object->_activated_at = OS::time();
					_object->_last_proof = _object->_activated_at;
					Transport::activate_link(*this);
					VERBOSEF("Link %s established with %s, RTT is %f s", toString().c_str(), _object->_destination.toString().c_str(), OS::round(_object->_rtt, 3));
					
					//p if _object->_rtt != None and _object->_establishment_cost != None and _object->_rtt > 0 and _object->_establishment_cost > 0:
					if (_object->_rtt != 0.0 && _object->_establishment_cost != 0 && _object->_rtt > 0 and _object->_establishment_cost > 0) {
						_object->_establishment_rate = _object->_establishment_cost / _object->_rtt;
					}

                    //p rtt_data = umsgpack.packb(self.rtt)
					MsgPack::Packer packer;
					packer.serialize(_object->_rtt);
					Bytes rtt_data(packer.data(), packer.size());
TRACEF("***** RTT data size: %d", rtt_data.size());
                    //p rtt_packet = RNS.Packet(self, rtt_data, context=RNS.Packet.LRRTT)
					Packet rtt_packet(*this, rtt_data, Type::Packet::DATA, Type::Packet::LRRTT);
TRACEF("***** RTT packet data: %s", rtt_packet.data().toHex().c_str());
rtt_packet.pack();
Packet test_packet(RNS::Destination(RNS::Type::NONE), rtt_packet.raw());
test_packet.unpack();
TRACEF("***** RTT test packet destination hash: %s", test_packet.destination_hash().toHex().c_str());
TRACEF("***** RTT test packet data size: %d", test_packet.data().size());
TRACEF("***** RTT test packet data: %s", test_packet.data().toHex().c_str());
Bytes plaintext = decrypt(test_packet.data());
TRACEF("***** RTT test packet plaintext: %s", plaintext.toHex().c_str());
					rtt_packet.send();
					had_outbound();

					if (_object->_callbacks._established != nullptr) {
						VERBOSEF("Link %s is established", link_id().toHex().c_str());
						//p thread = threading.Thread(target=_object->_callbacks.link_established, args=(self,))
						//p thread.daemon = True
						//p thread.start()
						_object->_callbacks._established(*this);
					}
				}
				else {
					DEBUGF("Invalid link proof signature received by %s. Ignoring.", toString().c_str());
				}
			}
			else {
				DEBUGF("Failed initiator/size check for link proof signature received by %s. Ignoring.", toString().c_str());
			}
		}
	}
	catch (std::exception& e) {
		_object->_status = Type::Link::CLOSED;
		ERRORF("An error ocurred while validating link request proof on %s.", toString().c_str());
		ERRORF("The contained exception was: %s", e.what());
	}
}


/*
Identifies the initiator of the link to the remote peer. This can only happen
once the link has been established, and is carried out over the encrypted link.
The identity is only revealed to the remote peer, and initiator anonymity is
thus preserved. This method can be used for authentication.

:param identity: An RNS.Identity instance to identify as.
*/
void Link::identify(const Identity& identity) {
	assert(_object);
	DEBUGF("Link %s requesting identity", link_id().toHex().c_str());
	if (_object->_initiator && _object->_status == Type::Link::ACTIVE) {
		const Bytes signed_data(_object->_link_id + identity.get_public_key());
		const Bytes signature(identity.sign(signed_data));
		const Bytes proof_data(identity.get_public_key() + signature);

		Packet proof(*this, proof_data, Type::Packet::DATA, Type::Packet::LINKIDENTIFY);
		proof.send();
		had_outbound();
	}
}

/*
Sends a request to the remote peer.

:param path: The request path.
:param response_callback: An optional function or method with the signature *response_callback(request_receipt)* to be called when a response is received. See the :ref:`Request Example<example-request>` for more info.
:param failed_callback: An optional function or method with the signature *failed_callback(request_receipt)* to be called when a request fails. See the :ref:`Request Example<example-request>` for more info.
:param progress_callback: An optional function or method with the signature *progress_callback(request_receipt)* to be called when progress is made receiving the response. Progress can be accessed as a float between 0.0 and 1.0 by the *request_receipt.progress* property.
:param timeout: An optional timeout in seconds for the request. If *None* is supplied it will be calculated based on link RTT.
:returns: A :ref:`RNS.RequestReceipt<api-requestreceipt>` instance if the request was sent, or *False* if it was not.
*/
const RNS::RequestReceipt Link::request(const Bytes& path, const Bytes& data /*= {Bytes::NONE}*/, RequestReceipt::Callbacks::response response_callback /*= nullptr*/, RequestReceipt::Callbacks::failed failed_callback /*= nullptr*/, RequestReceipt::Callbacks::progress progress_callback /*= nullptr*/, double timeout /*= 0.0*/) {
	assert(_object);
	DEBUGF("Link %s sending request", link_id().toHex().c_str());
	const Bytes request_path_hash(Identity::truncated_hash(path));

	//p unpacked_request = [OS::time(), request_path_hash, data]
	//p packed_request = umsgpack.packb(unpacked_request)
    MsgPack::Packer packer;
	packer.to_array(OS::time(), request_path_hash, data);
	Bytes packed_request(packer.data(), packer.size());

	if (timeout == 0.0) {
		timeout = _object->_rtt * _object->_traffic_timeout_factor + Type::Resource::RESPONSE_MAX_GRACE_TIME * 1.125;
	}

	if (packed_request.size() <= MDU) {
		Packet request_packet(*this, packed_request, Type::Packet::DATA, Type::Packet::REQUEST);
		PacketReceipt packet_receipt = request_packet.send();

		if (!packet_receipt) {
			return {Type::NONE};
		}
		else {
			packet_receipt.set_timeout(timeout);
			return RequestReceipt(
				*this,
				packet_receipt,
				{Type::NONE},
				response_callback,
				failed_callback,
				progress_callback,
				timeout,
				packed_request.size()
			);
		}
	}
	else {
		const Bytes request_id(Identity::truncated_hash(packed_request));
		DEBUGF("Sending request %s as resource.", request_id.toHex().c_str());
		Resource request_resource(packed_request, *this, request_id, false, timeout);

		return RequestReceipt(
			*this,
			{Type::NONE},
			request_resource,
			response_callback,
			failed_callback,
			progress_callback,
			timeout,
			packed_request.size()
		);
	}
}


void Link::update_mdu() {
	assert(_object);
	_object->_mdu = _object->_mtu - RNS::Type::Reticulum::HEADER_MAXSIZE - RNS::Type::Reticulum::IFAC_MIN_SIZE;
    //p self.mdu = math.floor((self.mtu-RNS.Reticulum.IFAC_MIN_SIZE-RNS.Reticulum.HEADER_MINSIZE-RNS.Identity.TOKEN_OVERHEAD)/RNS.Identity.AES128_BLOCKSIZE)*RNS.Identity.AES128_BLOCKSIZE - 1
	_object->_mdu = std::floor((_object->_mtu-RNS::Type::Reticulum::IFAC_MIN_SIZE-RNS::Type::Reticulum::HEADER_MINSIZE-RNS::Type::Identity::TOKEN_OVERHEAD)/RNS::Type::Identity::AES128_BLOCKSIZE)*RNS::Type::Identity::AES128_BLOCKSIZE - 1;
}

void Link::rtt_packet(const Packet& packet) {
	assert(_object);
	try {
		double measured_rtt = OS::time() - _object->_request_time;
		const Bytes plaintext(decrypt(packet.data()));
		if (plaintext) {
			//p rtt = umsgpack.unpackb(plaintext)
			MsgPack::Unpacker unpacker;
			unpacker.feed(plaintext.data(), plaintext.size());
			double rtt = 0.0;
			unpacker.deserialize(rtt);
			_object->_rtt = std::max(measured_rtt, rtt);
			_object->_status = Type::Link::ACTIVE;
			_object->_activated_at = OS::time();

			//p if _object->_rtt != None and _object->_establishment_cost != None and _object->_rtt > 0 and _object->_establishment_cost > 0:
			if (_object->_rtt != 0.0 && _object->_establishment_cost != 0.0 && _object->_rtt > 0 and _object->_establishment_cost > 0) {
				_object->_establishment_rate = _object->_establishment_cost / _object->_rtt;
			}

			try {
				if (_object->_owner.callbacks()._link_established != nullptr) {
					_object->_owner.callbacks()._link_established(*this);
				}
			}
			catch (std::exception& e) {
				ERRORF("Error occurred in external link establishment callback. The contained exception was: %s", e.what());
			}
		}
	}
	catch (std::exception& e) {
		ERRORF("Error occurred while processing RTT packet, tearing down link. The contained exception was: %s", e.what());
		teardown();
	}
}

/*
:returns: The data transfer rate at which the link establishment procedure ocurred, in bits per second.
*/
float Link::get_establishment_rate() {
	assert(_object);
	//p if _object->_establishment_rate != None:
	//p 	return _object->_establishment_rate*8
	//p else:
	//p 	return None
	return _object->_establishment_rate*8;
}

/*
:returns: The MTU of an established link.
*/
uint16_t Link::get_mtu() {
	assert(_object);
	if (_object->_status == ACTIVE) {
		return _object->_mtu;
	}
	return 0;
}

/*
:returns: The packet MDU of an established link.
*/
uint16_t Link::get_mdu() {
	assert(_object);
	if (_object->_status == ACTIVE) {
		return _object->_mdu;
	}
	return 0;
}

/*
:returns: The packet expected in-flight data rate of an established link.
*/
float Link::get_expected_rate() {
	assert(_object);
	if (_object->_status == ACTIVE) {
		return _object->_expected_rate;
	}
	return 0.0;
}

/*
:returns: The mode of an established link.
*/
link_mode Link::get_mode() {
	assert(_object);
	return _object->_mode;
}

const Bytes& Link::get_salt() {
	assert(_object);
	return _object->_link_id;
}

const Bytes Link::get_context() {
	return {Bytes::NONE};
}

/*
:returns: The time in seconds since this link was established.
*/
double Link::get_age() {
	assert(_object);
	if (_object->_activated_at) {
		return OS::time() - _object->_activated_at;
	}
	return 0.0;
}

/*
:returns: The time in seconds since last inbound packet on the link. This includes keepalive packets.
*/
double Link::no_inbound_for() {
	assert(_object);
	//p activated_at = _object->_activated_at if _object->_activated_at != None else 0
	double activated_at = _object->_activated_at;
	double last_inbound = std::max(_object->_last_inbound, activated_at);
	return (OS::time() - last_inbound);
}

/*
:returns: The time in seconds since last outbound packet on the link. This includes keepalive packets.
*/
double Link::no_outbound_for() {
	assert(_object);
	return OS::time() - _object->_last_outbound;
}

/*
:returns: The time in seconds since payload data traversed the link. This excludes keepalive packets.
*/
double Link::no_data_for() {
	assert(_object);
	return OS::time() - _object->_last_data;
}

/*
:returns: The time in seconds since activity on the link. This includes keepalive packets.
*/
double Link::inactive_for() {
	assert(_object);
	return std::min(no_inbound_for(), no_outbound_for());
}

/*
:returns: The identity of the remote peer, if it is known. Calling this method will not query the remote initiator to reveal its identity. Returns ``None`` if the link initiator has not already independently called the ``identify(identity)`` method.
*/
const Identity& Link::get_remote_identity() {
	assert(_object);
	return _object->__remote_identity;
}

void Link::had_outbound(bool is_keepalive /*= false*/) {
	assert(_object);
	_object->_last_outbound = OS::time();
	if (!is_keepalive) {
		_object->_last_data = _object->_last_outbound;
	}
}

/*
Closes the link and purges encryption keys. New keys will
be used if a new link to the same destination is established.
*/
void Link::teardown() {
	assert(_object);
	if (_object->_status != Type::Link::PENDING && _object->_status != Type::Link::CLOSED) {
		Packet teardown_packet(*this, _object->_link_id, Type::Packet::DATA, Type::Packet::LINKCLOSE);
		teardown_packet.send();
		had_outbound();
	}
	_object->_status = Type::Link::CLOSED;
	if (_object->_initiator) {
		_object->_teardown_reason = Type::Link::INITIATOR_CLOSED;
	}
	else {
		_object->_teardown_reason = Type::Link::DESTINATION_CLOSED;
	}
	link_closed();
}

void Link::teardown_packet(const Packet& packet) {
	assert(_object);
	try {
		Bytes plaintext = decrypt(packet.data());
		if (plaintext == _object->_link_id) {
			_object->_status = Type::Link::CLOSED;
			if (_object->_initiator) {
				_object->_teardown_reason = Type::Link::DESTINATION_CLOSED;
			}
			else {
				_object->_teardown_reason = Type::Link::INITIATOR_CLOSED;
			}
			link_closed();
		}
	}
	catch (std::exception& e) {
	}
}

void Link::link_closed() {
	assert(_object);
	for (auto& resource : _object->_incoming_resources) {
		const_cast<Resource&>(resource).cancel();
	}
	for (auto& resource : _object->_outgoing_resources) {
		const_cast<Resource&>(resource).cancel();
	}
	if (_object->_channel) {
		_object->_channel._shutdown();
	}

	_object->_prv.reset();
	_object->_pub.reset();
	_object->_pub_bytes.clear();
	_object->_shared_key.clear();
	_object->_derived_key.clear();

	if (_object->_destination) {
		if (_object->_destination.direction() == Type::Destination::IN) {
			if (_object->_destination.has_link(*this)) {
				_object->_destination.remove_link(*this);
			}
		}
	}

	if (_object->_callbacks._closed) {
		try {
			_object->_callbacks._closed(*this);
		}
		catch (std::exception& e) {
			ERRORF("Error while executing link closed callback from %s. The contained exception was: %s", toString().c_str(), e.what());
		}
	}
}

// CBA TODO Implement watchdog
void Link::start_watchdog() {
	//z thread = threading.Thread(target=_object->___watchdog_job)
	//z thread.daemon = True
	//z thread.start()
}

/*p TODO

void Link::__watchdog_job() {
	assert(_object);
	while not _object->_status == Type::Link::CLOSED:
		while (_object->_watchdog_lock):
			rtt_wait = 0.025
			if hasattr(self, "rtt") and _object->_rtt:
				rtt_wait = _object->_rtt

			sleep(max(rtt_wait, 0.025))

		if not _object->_status == Type::Link::CLOSED:
			# Link was initiated, but no response
			# from destination yet
			if _object->_status == PENDING:
				next_check = _object->_request_time + _object->_establishment_timeout
				sleep_time = next_check - OS::time()
				if OS::time() >= _object->_request_time + _object->_establishment_timeout:
					RNS.log("Link establishment timed out", RNS.LOG_VERBOSE)
					_object->_status = Type::Link::CLOSED
					_object->_teardown_reason = TIMEOUT
					link_closed()
					sleep_time = 0.001

			elif _object->_status == Type::Link::HANDSHAKE:
				next_check = _object->_request_time + _object->_establishment_timeout
				sleep_time = next_check - OS::time()
				if OS::time() >= _object->_request_time + _object->_establishment_timeout:
					_object->_status = Type::Link::CLOSED
					_object->_teardown_reason = TIMEOUT
					link_closed()
					sleep_time = 0.001

					if _object->_initiator:
						RNS.log("Timeout waiting for link request proof", RNS.LOG_DEBUG)
					else:
						RNS.log("Timeout waiting for RTT packet from link initiator", RNS.LOG_DEBUG)

			elif _object->_status == Type::Link::ACTIVE:
				activated_at = _object->_activated_at if _object->_activated_at != None else 0
				last_inbound = max(max(_object->_last_inbound, _object->_last_proof), activated_at)

				if OS::time() >= last_inbound + _object->_keepalive:
					if _object->_initiator:
						send_keepalive()

					if OS::time() >= last_inbound + _object->_stale_time:
						sleep_time = _object->_rtt * _object->_keepalive_timeout_factor + STALE_GRACE
						_object->_status = STALE
					else:
						sleep_time = _object->_keepalive
				
				else:
					sleep_time = (last_inbound + _object->_keepalive) - OS::time()

			elif _object->_status == STALE:
				sleep_time = 0.001
				_object->_status = Type::Link::CLOSED
				_object->_teardown_reason = TIMEOUT
				link_closed()


			if sleep_time == 0:
				RNS.log("Warning! Link watchdog sleep time of 0!", RNS.LOG_ERROR)
			if sleep_time == None or sleep_time < 0:
				RNS.log("Timing error! Tearing down link "+str(self)+" now.", RNS.LOG_ERROR)
				teardown()
				sleep_time = 0.1

			sleep(sleep_time)

*/

void Link::send_keepalive() {
	assert(_object);
    //p keepalive_packet = RNS.Packet(self, bytes([0xFF]), context=RNS.Packet.KEEPALIVE)
	RNS::Packet keepalive_packet(*this, Bytes("\xFF"), Type::Packet::DATA, Type::Packet::KEEPALIVE);
	keepalive_packet.send();
	had_outbound(true);
}

void Link::handle_request(const Bytes& request_id, const ResourceRequest& resource_request) {
	assert(_object);
	DEBUGF("Link %s handling request", link_id().toHex().c_str());
	if (_object->_status == Type::Link::ACTIVE) {
		//p requested_at = unpacked_request[0]
		//p path_hash    = unpacked_request[1]
		//p request_data = unpacked_request[2]

		auto handler_iter = _object->_destination.request_handlers().find(resource_request._path_hash);
		if (handler_iter != _object->_destination.request_handlers().end()) {
			TRACE("Link::handle_request: Found handler");
			RequestHandler request_handler = (*handler_iter).second;

			bool allowed = false;
			if (request_handler._allow != Type::Destination::ALLOW_NONE) {
				if (request_handler._allow == Type::Destination::ALLOW_LIST) {
					if (_object->__remote_identity && request_handler._allowed_list.count(_object->__remote_identity.hash()) > 0) {
						allowed = true;
					}
				}
				else if (request_handler._allow == Type::Destination::ALLOW_ALL) {
					allowed = true;
				}
			}

			if (allowed) {
				DEBUGF("Handling request %s for: %s", request_id.toHex().c_str(), request_handler._path.toString().c_str());
				//p if len(inspect.signature(response_generator).parameters) == 5:
				//p 	response = response_generator(path, request_data, request_id, _object->__remote_identity, requested_at)
				//p elif len(inspect.signature(response_generator).parameters) == 6:
				//p 	response = response_generator(path, request_data, request_id, _object->_link_id, _object->__remote_identity, requested_at)
				//p else:
				//p 	raise TypeError("Invalid signature for response generator callback")
				Bytes response(request_handler._response_generator(request_handler._path, resource_request._request_data, request_id, _object->_link_id, _object->__remote_identity, resource_request._requested_at));

				if (response) {
					//p packed_response = umsgpack.packb([request_id, response])
					MsgPack::Packer packer;
					packer.to_array(request_id, response);
					Bytes packed_response(packer.data(), packer.size());

					if (packed_response.size() <= MDU) {
						//p RNS.Packet(self, packed_response, Type::Packet::DATA, context = Type::Packet::RESPONSE).send()
						RNS::Packet response_packet(*this, packed_response, Type::Packet::DATA, Type::Packet::RESPONSE);
						response_packet.send();
					}
					else {
						// CBA TODO Determine why unused Resource is created here
						Resource response_resource = RNS::Resource(packed_response, *this, request_id, true);
					}
				}
			}
			else {
				std::string identity_string;
				if (get_remote_identity()) {
					identity_string = get_remote_identity().toString();
				}
				else {
					identity_string = "<Unknown>";
				}
				DEBUGF("Request %s from %s not allowed for: %s", request_id.toHex().c_str(), identity_string.c_str(), request_handler._path.toString().c_str());
			}
		}
	}
}

void Link::handle_response(const Bytes& request_id, const Bytes& response_data, size_t response_size, size_t response_transfer_size) {
	assert(_object);
	if (_object->_status == Type::Link::ACTIVE) {
		RNS::RequestReceipt remove = {Type::NONE};
		for (RNS::RequestReceipt pending_request : _object->_pending_requests) {
			if (pending_request.request_id() == request_id) {
				remove = pending_request;
				try {
					pending_request.response_size(response_size);
					//if (pending_request.response_transfer_size == 0) {
					//	pending_request.response_transfer_size = 0;
					//}
					pending_request.response_transfer_size(pending_request.response_transfer_size() + response_transfer_size);
					pending_request.response_received(response_data);
				}
				catch (std::exception& e) {
					ERRORF("Error occurred while handling response. The contained exception was: %s", e.what());
				}
				break;
			}
		}
		if (remove) {
			if (_object->_pending_requests.count(remove) > 0) {
				_object->_pending_requests.erase(remove);
			}
		}
	}
}

void Link::request_resource_concluded(const Resource& resource) {
	assert(_object);
	if (resource.status() == Type::Resource::COMPLETE) {
		//p packed_request = resource.data().read()
		Bytes packed_request = resource.data();
		//p unpacked_request = umsgpack.unpackb(packed_request)
		MsgPack::Unpacker unpacker;
		unpacker.feed(packed_request.data(), packed_request.size());
		// CBA TODO OPTIMIZE MSGPACK
		double requested_at;
		MsgPack::bin_t<uint8_t> path_hash;
		MsgPack::bin_t<uint8_t> request_data;
		unpacker.from_array(requested_at, path_hash, request_data);
		ResourceRequest resource_request;
		resource_request._requested_at = requested_at;
		resource_request._path_hash = path_hash;
		resource_request._request_data = request_data;
        //p request_id        = RNS.Identity.truncated_hash(packed_request)
		Bytes request_id(Identity::truncated_hash(resource.data()));
		//p request_data = unpacked_request

		//p handle_request(request_id, request_data)
		handle_request(request_id, resource_request);
	}
	else {
		DEBUGF("Incoming request resource failed with status: %d", resource.status());
	}
}

void Link::response_resource_concluded(const Resource& resource) {
	assert(_object);
	if (resource.status() == Type::Resource::COMPLETE) {
		//p packed_response = resource.data.read()
		Bytes packed_response = resource.data();
		//p unpacked_response = umsgpack.unpackb(packed_response)
		//p request_id        = unpacked_response[0]
		//p response_data     = unpacked_response[1]
		MsgPack::Unpacker unpacker;
		unpacker.feed(packed_response.data(), packed_response.size());
		MsgPack::bin_t<uint8_t> request_id;
		MsgPack::bin_t<uint8_t> response_data;
		unpacker.from_array(request_id, response_data);

		handle_response(request_id, response_data, resource.total_size(), resource.size());
	}
	else {
		DEBUGF("Incoming response resource failed with status: %d", resource.status());
		for (RNS::RequestReceipt pending_request : _object->_pending_requests) {
			if (pending_request.request_id() == resource.request_id()) {
				pending_request.request_timed_out({Type::NONE});
			}
		}
	}
}


/*z
"""
Get the ``Channel`` for this link.

:return: ``Channel`` object
"""
void Link::get_channel() {
	assert(_object);
	if _object->_channel is None:
		_object->_channel = Channel(LinkChannelOutlet(self))
	return _object->_channel
*/

/*
void Link::receive(const Packet& packet) {
}
*/
void Link::receive(const Packet& packet) {
	assert(_object);
	_object->_watchdog_lock = true;
	if (_object->_status != Type::Link::CLOSED && !(_object->_initiator && packet.context() == Type::Packet::KEEPALIVE && packet.data() == "\xFF")) {
		if (packet.receiving_interface() != _object->_attached_interface) {
			ERROR("Link-associated packet received on unexpected interface! Someone might be trying to manipulate your communication!");
		}
		else {
			_object->_last_inbound = OS::time();
			if (packet.context() != Type::Packet::KEEPALIVE) {
				_object->_last_data = _object->_last_inbound;
			}
			_object->_rx += 1;
			_object->_rxbytes += packet.data().size();
			if (_object->_status == STALE) {
				_object->_status = Type::Link::ACTIVE;
			}

			if (packet.packet_type() == Type::Packet::DATA) {
				bool should_query = false;
				switch (packet.context()) {
				case Type::Packet::CONTEXT_NONE:
				{
					const Bytes plaintext = decrypt(packet.data());
					if (plaintext) {
						if (_object->_callbacks._packet) {
							//z thread = threading.Thread(target=_object->_callbacks.packet, args=(plaintext, packet))
							//z thread.daemon = True
							//z thread.start()
							try {
								_object->_callbacks._packet(plaintext, packet);
							}
							catch (std::exception& e) {
								ERRORF("Error while executing packet callback from %s. The contained exception was: %s", toString().c_str(), e.what());
							}
						}
						
						if (_object->_destination.proof_strategy() == Type::Destination::PROVE_ALL) {
							const_cast<Packet&>(packet).prove();
							should_query = true;
						}
						else if (_object->_destination.proof_strategy() == Type::Destination::PROVE_APP) {
							if (_object->_destination.callbacks()._proof_requested) {
								try {
									if (_object->_destination.callbacks()._proof_requested(packet)) {
										const_cast<Packet&>(packet).prove();
										should_query = true;
									}
								}
								catch (std::exception& e) {
									ERRORF("Error while executing proof request callback from %s. The contained exception was: %s", toString().c_str(), e.what());
								}
							}
						}
					}
					break;
				}
				case Type::Packet::LINKIDENTIFY:
				{
					const Bytes plaintext = decrypt(packet.data());
					if (plaintext) {
						if (_object->_initiator && plaintext.size() == Type::Identity::KEYSIZE/8 + Type::Identity::SIGLENGTH/8) {
							const Bytes public_key   = plaintext.left(Type::Identity::KEYSIZE/8);
							const Bytes signed_data  = _object->_link_id + public_key;
							const Bytes signature    = plaintext.mid(Type::Identity::KEYSIZE/8, Type::Identity::SIGLENGTH/8);
							Identity identity(false);
							identity.load_public_key(public_key);

							if (identity.validate(signature, signed_data)) {
								_object->__remote_identity = identity;
								if (_object->_callbacks._remote_identified) {
									try {
										_object->_callbacks._remote_identified(*this, _object->__remote_identity);
									}
									catch (std::exception& e) {
										ERRORF("Error while executing remote identified callback from %s. The contained exception was: %s", toString().c_str(), e.what());
									}
								}
							}
						}
					}
					break;
				}
				case Type::Packet::REQUEST:
				{
					try {
						const Bytes request_id = packet.getTruncatedHash();
						const Bytes packed_request = decrypt(packet.data());
						if (packed_request) {
                            //p unpacked_request = umsgpack.unpackb(packed_request)
							MsgPack::Unpacker unpacker;
							unpacker.feed(packed_request.data(), packed_request.size());
							// CBA TODO OPTIMIZE MSGPACK
							double requested_at;
							MsgPack::bin_t<uint8_t> path_hash;
							MsgPack::bin_t<uint8_t> request_data;
							unpacker.from_array(requested_at, path_hash, request_data);
							ResourceRequest resource_request;
							resource_request._requested_at = requested_at;
							resource_request._path_hash = path_hash;
							resource_request._request_data = request_data;
							handle_request(request_id, resource_request);
						}
					}
					catch (std::exception& e) {
						ERRORF("Error occurred while handling request. The contained exception was: %s", e.what());
					}
					break;
				}
				case Type::Packet::RESPONSE:
				{
					try {
						const Bytes packed_response = decrypt(packet.data());
						if (packed_response) {
							//p unpacked_response = umsgpack.unpackb(packed_response)
							//p request_id = unpacked_response[0]
							//p response_data = unpacked_response[1]
                            //p transfer_size = len(umsgpack.packb(response_data))-2
							MsgPack::Unpacker unpacker;
							unpacker.feed(packed_response.data(), packed_response.size());
							MsgPack::bin_t<uint8_t> request_id;
							MsgPack::bin_t<uint8_t> response_data;
							unpacker.from_array(request_id, response_data);
							MsgPack::Packer packer;
							packer.serialize(response_data);
							size_t transfer_size = packer.size() - 2;
							handle_response(Bytes(request_id.data(), request_id.size()), Bytes(response_data.data(), response_data.size()), transfer_size, transfer_size);
						}
					}
					catch (std::exception& e) {
						ERRORF("Error occurred while handling response. The contained exception was: %s", e.what());
					}
					break;
				}
				case Type::Packet::LRRTT:
				{
					if (!_object->_initiator) {
						rtt_packet(packet);
					}
				}
				case Type::Packet::LINKCLOSE:
				{
					teardown_packet(packet);
					break;
				}
/*z
				case Type::Packet::RESOURCE_ADV:
				{
					//p packet.plaintext = decrypt(packet.data)
					const Bytes plaintext = decrypt(packet.data());
					if (plaintext) {
						const_cast<Packet&>(packet).plaintext(plaintext);
						if (ResourceAdvertisement::is_request(packet)) {
							Resource::accept(packet, callback=_object->_request_resource_concluded);
						}
						else if (ResourceAdvertisement::is_response(packet)) {
							Bytes request_id = ResourceAdvertisement::read_request_id(packet)
							for (auto& pending_request : _object->_pending_requests) {
								if (pending_request.request_id == request_id) {
									const Bytes response_resource = Resource::accept(packet, callback=_object->_response_resource_concluded, progress_callback=pending_request.response_resource_progress, request_id = request_id);
									if (response_resource) {
										//p if pending_request.response_size == None:
										if (pending_request.response_size == 0) {
											pending_request.response_size = ResourceAdvertisement::read_size(packet);
										}
										//p if pending_request.response_transfer_size == None:
										if (pending_request.response_transfer_size == 0) {
											pending_request.response_transfer_size = 0;
										}
										pending_request.response_transfer_size += ResourceAdvertisement::read_transfer_size(packet);
										//p if pending_request.started_at == None:
										if (pending_request.started_at == 0.0) {
											pending_request.started_at = OS::time();
										}
										pending_request.response_resource_progress(response_resource);
									}
								}
							}
						}
						else if (_object->_resource_strategy == ACCEPT_NONE) {
							//p pass
						}
						else if (_object->_resource_strategy == ACCEPT_APP) {
							if (_object->_callbacks.resource) {
								try {
									resource_advertisement = RNS.ResourceAdvertisement.unpack(packet.plaintext());
									resource_advertisement.link = *this;
									if (_object->_callbacks.resource(resource_advertisement)) {
										Resource::accept(packet, _object->_callbacks.resource_concluded);
									}
								}
								catch (std::exception& e) {
									ERRORF("Error while executing resource accept callback from %s. The contained exception was: %s", toString().c_str(), e.what());
								}
						elif _object->_resource_strategy == ACCEPT_ALL:
							RNS.Resource.accept(packet, _object->_callbacks.resource_concluded)
					break;
				}
				case Type::Packet::RESOURCE_REQ:
				{
					const Bytes plaintext = decrypt(packet.data());
					if (plaintext) {
						if ord(plaintext[:1]) == RNS.Resource.HASHMAP_IS_EXHAUSTED:
							resource_hash = plaintext[1+RNS.Resource.MAPHASH_LEN:Type::Identity::HASHLENGTH//8+1+RNS.Resource.MAPHASH_LEN]
						else:
							resource_hash = plaintext[1:Type::Identity::HASHLENGTH//8+1]

						for resource in _object->_outgoing_resources:
							if resource.hash == resource_hash:
								// We need to check that this request has not been
								// received before in order to avoid sequencing errors.
								if not packet.packet_hash in resource.req_hashlist:
									resource.req_hashlist.append(packet.packet_hash)
									resource.request(plaintext)
					break;
				}
				case Type::Packet::RESOURCE_HMU:
				{
					const Bytes plaintext = decrypt(packet.data());
					if (plaintext) {
						resource_hash = plaintext[:Type::Identity::HASHLENGTH//8]
						for resource in _object->_incoming_resources:
							if resource_hash == resource.hash:
								resource.hashmap_update_packet(plaintext)
					break;
				}
				case Type::Packet::RESOURCE_ICL:
				{
					const Bytes plaintext = decrypt(packet.data());
					if (plaintext) {
						resource_hash = plaintext[:Type::Identity::HASHLENGTH//8]
						for resource in _object->_incoming_resources:
							if resource_hash == resource.hash:
								resource.cancel()
					break;
				}
*/
				case Type::Packet::KEEPALIVE:
				{
					if (!_object->_initiator && packet.data() == "\xFF") {
                        //p keepalive_packet = RNS.Packet(self, bytes([0xFE]), context=RNS.Packet.KEEPALIVE)
						Packet keepalive_packet(*this, Bytes("\xFE"), Type::Packet::DATA, Type::Packet::KEEPALIVE);
						keepalive_packet.send();
						had_outbound(true);
					}
					break;
				}
				// TODO: find the most efficient way to allow multiple
				// transfers at the same time, sending resource hash on
				// each packet is a huge overhead. Probably some kind
				// of hash -> sequence map
				case Type::Packet::RESOURCE:
				{
					for (auto& resource : _object->_incoming_resources) {
						//z resource.receive_part(packet);
					}
					break;
				}
/*z
				case Type::Packet::CHANNEL:
				{
					//z if (!_object->_channel) {
					if (true) {
						DEBUG(f"Channel data received without open channel")
					}
					else {
						//z packet.prove();
						//z plaintext = decrypt(packet.data());
						//z if (plaintext) {
						//z 	_object->_channel._receive(plaintext);
						//z }
					}
					break;
				}
*/
				}
			}
			else if (packet.packet_type() == Type::Packet::PROOF) {
				if (packet.context() == Type::Packet::RESOURCE_PRF) {
					Bytes resource_hash = packet.data().left(Type::Identity::HASHLENGTH/8);
					for (const auto& resource : _object->_outgoing_resources) {
						if (resource_hash == resource.hash()) {
							//z resource.validate_proof(packet.data());
						}
					}
				}
			}
		}
	}

	_object->_watchdog_lock = false;
}

const Bytes Link::encrypt(const Bytes& plaintext) {
	assert(_object);
	TRACE("Link::encrypt: encrypting data...");
	try {
		if (!_object->_token) {
			try {
				_object->_token.reset(new Token(_object->_derived_key));
			}
			catch (std::exception& e) {
				ERRORF("Could not instantiate Token while performing encryption on link %s. The contained exception was: %s", toString().c_str(), e.what());
				throw e;
			}
		}
		return _object->_token->encrypt(plaintext);
	}
	catch (std::exception& e) {
		ERRORF("Encryption on link %s failed. The contained exception was: %s", toString().c_str(), e.what());
		throw e;
	}
}

const Bytes Link::decrypt(const Bytes& ciphertext) {
	assert(_object);
	TRACE("Link::decrypt: decrypting data...");
	try {
		if (!_object->_token) {
			_object->_token.reset(new Token(_object->_derived_key));
		}
		return _object->_token->decrypt(ciphertext);
	}
	catch (std::exception& e) {
		ERRORF("Decryption failed on link %s. The contained exception was: %s", toString().c_str(), e.what());
		return {Bytes::NONE};
	}
}

const Bytes Link::sign(const Bytes& message) {
	assert(_object);
	return _object->_sig_prv->sign(message);
}

bool Link::validate(const Bytes& signature, const Bytes& message) {
	assert(_object);
	try {
		_object->_peer_sig_pub->verify(signature, message);
		return true;
	}
	catch (std::exception& e) {
		return false;
	}
}

void Link::set_link_established_callback(Callbacks::established callback) {
	assert(_object);
	_object->_callbacks._established = callback;
}

void Link::set_link_closed_callback(Callbacks::closed callback) {
	assert(_object);
	_object->_callbacks._closed = callback;
}

void Link::set_packet_callback(Callbacks::packet callback) {
	assert(_object);
	_object->_callbacks._packet = callback;
}

void Link::set_remote_identified_callback(Callbacks::remote_identified callback) {
	assert(_object);
	_object->_callbacks._remote_identified = callback;
}

void Link::set_resource_callback(Callbacks::resource callback) {
	assert(_object);
	_object->_callbacks._resource = callback;
}

void Link::set_resource_started_callback(Callbacks::resource_started callback) {
	assert(_object);
	_object->_callbacks._resource_started = callback;
}

void Link::set_resource_concluded_callback(Callbacks::resource_concluded callback) {
	assert(_object);
	_object->_callbacks._resource_concluded = callback;
}


void Link::resource_concluded(const Resource& resource) {
	assert(_object);
	if (_object->_incoming_resources.count(resource) > 0) {
		_object->_incoming_resources.erase(resource);
	}
	if (_object->_outgoing_resources.count(resource) > 0) {
		_object->_outgoing_resources.erase(resource);
	}
}

/*
Sets the resource strategy for the link.

:param resource_strategy: One of ``RNS.ACCEPT_NONE``, ``RNS.ACCEPT_ALL`` or ``RNS.ACCEPT_APP``. If ``RNS.ACCEPT_APP`` is set, the `resource_callback` will be called to determine whether the resource should be accepted or not.
:raises: *TypeError* if the resource strategy is unsupported.
*/
void Link::set_resource_strategy(resource_strategy strategy) {
	assert(_object);
	if (!(strategy & resource_strategies)) {
		throw std::runtime_error("Unsupported resource strategy");
	}
	else {
		_object->_resource_strategy = strategy;
	}
}

void Link::register_outgoing_resource(const Resource& resource) {
	assert(_object);
	_object->_outgoing_resources.insert(resource);
}

void Link::register_incoming_resource(const Resource& resource) {
	assert(_object);
	_object->_incoming_resources.insert(resource);
}

bool Link::has_incoming_resource(const Resource& resource) {
	assert(_object);
	for (const auto& incoming_resource : _object->_incoming_resources) {
		if (incoming_resource.hash() == resource.hash()) {
			return true;
		}
	}
	return false;
}

void Link::cancel_outgoing_resource(const Resource& resource) {
	assert(_object);
	if (_object->_outgoing_resources.count(resource) > 0) {
		_object->_outgoing_resources.erase(resource);
	}
	else {
		ERROR("Attempt to cancel a non-existing outgoing resource");
	}
}

void Link::cancel_incoming_resource(const Resource& resource) {
	assert(_object);
	if (_object->_incoming_resources.count(resource)) {
		_object->_incoming_resources.erase(resource);
	}
	else {
		ERROR("Attempt to cancel a non-existing incoming resource");
	}
}

bool Link::ready_for_new_resource() {
	assert(_object);
	return (_object->_outgoing_resources.size() > 0);
}

std::string Link::toString() const {
	if (!_object) {
		return "";
	}
	return "{Link:" + _object->_link_id.toHex() + "}";
}

// getters

double Link::rtt() const {
	assert(_object);
	return _object->_rtt;
}

const Destination& Link::destination() const {
	assert(_object);
	return _object->_destination;
}

// CBA LINK
/*
const Destination& Link::link_destination() const {
	assert(_object);
	return _object->_link_destination;
}
*/

const Bytes& Link::link_id() const {
	assert(_object);
	return _object->_link_id;
}

const Bytes& Link::hash() const {
	assert(_object);
	return _object->_hash;
}

uint16_t Link::mtu() const {
	assert(_object);
	return _object->_mtu;
}

Type::Link::status Link::status() const {
	assert(_object);
	return _object->_status;
}

double Link::establishment_timeout() const {
	assert(_object);
	return _object->_establishment_timeout;
}

uint16_t Link::establishment_cost() const {
	assert(_object);
	return _object->_establishment_cost;
}

uint8_t Link::traffic_timeout_factor() const {
	assert(_object);
	return _object->_traffic_timeout_factor;
}

double Link::request_time() const {
	assert(_object);
	return _object->_request_time;
}

double Link::last_inbound() const {
	assert(_object);
	return _object->_last_inbound;
}

std::set<RNS::RequestReceipt>& Link::pending_requests() const {
	assert(_object);
	return _object->_pending_requests;
}

Type::Link::teardown_reason Link::teardown_reason() const {
	assert(_object);
	return _object->_teardown_reason;
}

bool Link::initiator() const {
	assert(_object);
	return _object->_initiator;
}

// setters

void Link::destination(const Destination& destination) {
	assert(_object);
	_object->_destination = destination;
}

void Link::attached_interface(const Interface& interface) {
	assert(_object);
	_object->_attached_interface = interface;
}

void Link::establishment_timeout(double timeout) {
	assert(_object);
	_object->_establishment_timeout = timeout;
}

void Link::establishment_cost(uint16_t cost) {
	assert(_object);
	_object->_establishment_cost = cost;
}

void Link::request_time(double time) {
	assert(_object);
	_object->_request_time = time;
}

void Link::last_inbound(double time) {
	assert(_object);
	_object->_last_inbound = time;
}

void Link::last_outbound(double time) {
	assert(_object);
	_object->_last_outbound = time;
}

void Link::increment_tx() {
	assert(_object);
	_object->_tx++;
}

void Link::increment_txbytes(uint16_t bytes) {
	assert(_object);
	_object->_txbytes += bytes;
}

void Link::status(Type::Link::status status) {
	assert(_object);
	_object->_status = status;
}


//RequestReceipt::RequestReceipt(const Link& link, const PacketReceipt& packet_receipt /*= {Type::NONE}*/, const Resource& resource /*= {Type::NONE}*/, RequestReceipt::Callbacks::response response_callback /*= nullptr*/, RequestReceipt::Callbacks::failed failed_callback /*= nullptr*/, RequestReceipt::Callbacks::progress progress_callback /*= nullptr*/, double timeout /*= 0.0*/, int request_size /*= 0*/) :
RequestReceipt::RequestReceipt(const Link& link, const PacketReceipt& packet_receipt, const Resource& resource, RequestReceipt::Callbacks::response response_callback /*= nullptr*/, RequestReceipt::Callbacks::failed failed_callback /*= nullptr*/, RequestReceipt::Callbacks::progress progress_callback /*= nullptr*/, double timeout /*= 0.0*/, int request_size /*= 0*/) :
	_object(new RequestReceiptData())
{
	assert(_object);
	_object->_packet_receipt = packet_receipt;
	_object->_resource = resource;
	if (_object->_packet_receipt) {
		_object->_hash = packet_receipt.truncated_hash();
		//z _object->_packet_receipt.set_timeout_callback(request_timed_out);
		_object->_started_at = OS::time();
	}
	else if (_object->_resource) {
		_object->_hash = resource.request_id();
		//const_cast<Resource&>(resource).set_concluded_callback(request_resource_concluded);
		const_cast<Resource&>(resource).set_concluded_callback(std::bind(&RequestReceipt::request_resource_concluded, this, std::placeholders::_1));
	}
	_object->_link = link;
	_object->_request_id = _object->_hash;
	_object->_request_size = request_size;
	_object->_sent_at = OS::time();
	if (timeout != 0.0) {
		_object->_timeout = timeout;
	}
	else {
		throw new std::invalid_argument("No timeout specified for request receipt");
	}

	_object->_callbacks._response = response_callback;
	_object->_callbacks._failed = failed_callback;
	_object->_callbacks._progress = progress_callback;

	_object->_link.pending_requests().insert(*this);
}

void RequestReceipt::request_resource_concluded(const Resource& resource) {
	assert(_object);
	if (resource.status() == Type::Resource::COMPLETE) {
		DEBUGF("Request %s successfully sent as resource.", _object->_request_id.toHex().c_str());
		if (_object->_started_at == 0.0) {
			_object->_started_at = OS::time();
		}
		_object->_status = Type::RequestReceipt::DELIVERED;
		_object->_resource_response_timeout = OS::time() + _object->_timeout;
		//p response_timeout_thread = threading.Thread(target=_object->___response_timeout_job)
		//p response_timeout_thread.daemon = True
		//p response_timeout_thread.start()
	}
	else {
		DEBUGF("Sending request %s as resource failed with status: %d", _object->_request_id.toHex().c_str(), resource.status());
		_object->_status = Type::RequestReceipt::FAILED;
		_object->_concluded_at = OS::time();
		_object->_link.pending_requests().erase(*this);
		if (_object->_callbacks._failed != nullptr) {
			try {
				_object->_callbacks._failed(*this);
			}
			catch (std::exception& e) {
				ERRORF("Error while executing request failed callback from %s. The contained exception was: %s", toString().c_str(), e.what());
			}
		}
	}
}


void RequestReceipt::__response_timeout_job() {
	assert(_object);
	while (_object->_status == Type::RequestReceipt::DELIVERED) {
		double now = OS::time();
		if (now > _object->___resource_response_timeout) {
			request_timed_out({Type::NONE});
		}

		OS::sleep(0.1);
	}
}


void RequestReceipt::request_timed_out(const PacketReceipt& packet_receipt) {
	assert(_object);
	_object->_status = Type::RequestReceipt::FAILED;
	_object->_concluded_at = OS::time();
	_object->_link.pending_requests().erase(*this);

	if (_object->_callbacks._failed != nullptr) {
		try {
			_object->_callbacks._failed(*this);
		}
		catch (std::exception& e) {
			ERRORF("Error while executing request timed out callback from %s. The contained exception was: %s", toString().c_str(), e.what());
		}
	}
}


void RequestReceipt::response_resource_progress(const Resource& resource) {
	assert(_object);
	if (resource) {
		if (_object->_status != Type::RequestReceipt::FAILED) {
			_object->_status = Type::RequestReceipt::RECEIVING;
			if (_object->_packet_receipt) {
				if (_object->_packet_receipt.status() != Type::PacketReceipt::DELIVERED) {
					_object->_packet_receipt.status(Type::PacketReceipt::DELIVERED);
					_object->_packet_receipt.proved(true);
					_object->_packet_receipt.concluded_at(OS::time());
					if (_object->_packet_receipt.callbacks()._delivery != nullptr) {
						_object->_packet_receipt.callbacks()._delivery(_object->_packet_receipt);
					}
				}
			}

			_object->_progress = resource.get_progress();

			if (_object->_callbacks._progress != nullptr) {
				try {
					_object->_callbacks._progress(*this);
				}
				catch (std::exception& e) {
					ERRORF("Error while executing response progress callback from %s. The contained exception was: %s", toString().c_str(), e.what());
				}
			}
		}
		else {
			const_cast<Resource&>(resource).cancel();
		}
	}
}

void RequestReceipt::response_received(const Bytes& response) {
	assert(_object);
	if (_object->_status != Type::RequestReceipt::FAILED) {
		_object->_progress = 1.0;
		_object->_response = response;
		_object->_status = Type::RequestReceipt::READY;
		_object->_response_concluded_at = OS::time();

		if (_object->_packet_receipt) {
			_object->_packet_receipt.status(Type::PacketReceipt::DELIVERED);
			_object->_packet_receipt.proved(true);
			_object->_packet_receipt.concluded_at(OS::time());
			if (_object->_packet_receipt.callbacks()._delivery != nullptr) {
				_object->_packet_receipt.callbacks()._delivery(_object->_packet_receipt);
			}
		}
		if (_object->_callbacks._progress != nullptr) {
			try {
				_object->_callbacks._progress(*this);
			}
			catch (std::exception& e) {
				ERRORF("Error while executing response progress callback from %s. The contained exception was: %s", toString().c_str(), e.what());
			}
		}
		if (_object->_callbacks._response != nullptr) {
			try {
				_object->_callbacks._response(*this);
			}
			catch (std::exception& e) {
				ERRORF("Error while executing response received callback from %s. The contained exception was: %s", toString().c_str(), e.what());
			}
		}
	}
}

// :returns: The request ID as *bytes*.
const Bytes& RequestReceipt::get_request_id() const {
	assert(_object);
	return _object->_request_id;
}

// :returns: The current status of the request, one of ``RNS.RequestReceipt.FAILED``, ``RNS.RequestReceipt.SENT``, ``RNS.RequestReceipt.DELIVERED``, ``RNS.RequestReceipt.READY``.
Type::RequestReceipt::status RequestReceipt::get_status() const {
	assert(_object);
	return _object->_status;
}

// :returns: The progress of a response being received as a *float* between 0.0 and 1.0.
float RequestReceipt::get_progress() const {
	assert(_object);
	return _object->_progress;
}

// :returns: The response as *bytes* if it is ready, otherwise *None*.
const Bytes RequestReceipt::get_response() const {
	assert(_object);
	if (_object->_status == Type::RequestReceipt::READY) {
		return _object->_response;
	}
	else {
		return Bytes::NONE;
	}
}

// :returns: The response time of the request in seconds.
double RequestReceipt::get_response_time() const {
	assert(_object);
	if (_object->_status == Type::RequestReceipt::READY) {
		return _object->_response_concluded_at - _object->_started_at;
	}
	else {
		return 0.0;
	}
}

std::string RequestReceipt::toString() const {
	if (!_object) {
		return "";
	}
	return "{RequestReceipt:" + _object->_hash.toHex() + "}";
}

// getters

const Bytes& RequestReceipt::hash() const {
	assert(_object);
	return _object->_hash;
}

const Bytes& RequestReceipt::request_id() const {
	assert(_object);
	return _object->_request_id;
}

size_t RequestReceipt::response_transfer_size() const {
	assert(_object);
	return _object->_response_transfer_size;
}

// setters

void RequestReceipt::response_size(size_t size) {
	assert(_object);
	_object->_response_size = size;
}

void RequestReceipt::response_transfer_size(size_t size) {
	assert(_object);
	_object->_response_transfer_size = size;
}
