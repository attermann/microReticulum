#pragma once

//#include "LinkCallbacks.h"
#include "Link.h"

#include "Resource.h"
#include "Channel.h"
#include "Interface.h"
#include "Packet.h"
#include "Destination.h"
#include "Bytes.h"
#include "Type.h"
#include "Cryptography/Token.h"

#include <set>

namespace RNS {

	class LinkData {
	public:
		LinkData(const Destination& destination) : _destination(destination) {
			MEM("LinkData object copy created");
		}
		virtual ~LinkData() {
			MEM("LinkData object destroyed");
		}
	private:
		Destination _destination;

		// CBA LINK
		Destination _link_destination = {Type::NONE};

		Bytes _link_id;
		Bytes _hash;
		Type::Link::status _status = Type::Link::PENDING;

		RNS::Type::Link::link_mode _mode = Link::MODE_DEFAULT;
		double _rtt = 0.0;
		uint16_t _mtu = RNS::Type::Reticulum::MTU;
		uint16_t _mdu = 0;
		uint16_t _establishment_cost = 0;
		Link::Callbacks _callbacks;
		Type::Link::resource_strategy _resource_strategy = Type::Link::ACCEPT_NONE;
		double _last_inbound = 0.0;
		double _last_outbound = 0.0;
		double _last_proof = 0.0;
		double _last_data = 0.0;
		uint16_t _tx = 0;
		uint16_t _rx = 0;
		uint16_t _txbytes = 0;
		uint16_t _rxbytes = 0;
		float _rssi = 0.0;
		float _snr = 0.0;
		float _q = 0.0;
		uint16_t _traffic_timeout_factor = Type::Link::TRAFFIC_TIMEOUT_FACTOR;
		uint16_t _keepalive_timeout_factor = Type::Link::KEEPALIVE_TIMEOUT_FACTOR;
		uint16_t _keepalive = Type::Link::KEEPALIVE;
		uint16_t _stale_time = Type::Link::STALE_TIME;
		bool _watchdog_lock = false;
		double _activated_at = 0.0;
		Type::Destination::types _type = Type::Destination::LINK;
		Destination _owner = {Type::NONE};
		bool _initiator = false;
		uint8_t _expected_hops = 0;
		Interface _attached_interface = {Type::NONE};
		Identity __remote_identity = {Type::NONE};
		Channel _channel = {Type::NONE};
		double _establishment_timeout = 0.0;
		Bytes _request_data;
		Packet _packet = {Type::NONE};
		double _request_time = 0.0;
		float _establishment_rate = 0.0;
        float _expected_rate = 0.0;
		Type::Link::teardown_reason _teardown_reason = Type::Link::TEARDOWN_NONE;

		Cryptography::Token::Ptr _token;

		Cryptography::X25519PrivateKey::Ptr _prv;
		Bytes _prv_bytes;

		Cryptography::Ed25519PrivateKey::Ptr _sig_prv;
		Bytes _sig_prv_bytes;

		Cryptography::X25519PublicKey::Ptr _pub;
		Bytes _pub_bytes;

		Cryptography::Ed25519PublicKey::Ptr _sig_pub;
		Bytes _sig_pub_bytes;

		Cryptography::X25519PublicKey::Ptr _peer_pub;
		Bytes _peer_pub_bytes;

		Cryptography::Ed25519PublicKey::Ptr _peer_sig_pub;
		Bytes _peer_sig_pub_bytes;

		Bytes _shared_key;
		Bytes _derived_key;

		std::set<Resource> _incoming_resources;
		std::set<Resource> _outgoing_resources;
		std::set<RNS::RequestReceipt> _pending_requests;

	friend class Link;
	};

	class RequestReceiptData {
	public:
		RequestReceiptData() {}
		virtual ~RequestReceiptData() {}
	private:
		Bytes _hash;
		PacketReceipt _packet_receipt = {Type::NONE};
		Resource _resource = {Type::NONE};
		Link _link;
		double _started_at = 0.0;
		Bytes _request_id;
		int _request_size = 0;
		Bytes _response;
		size_t _response_transfer_size = 0;
		size_t _response_size = 0;
		Type::RequestReceipt::status _status = Type::RequestReceipt::SENT;
		double _sent_at = 0.0;
		int _progress = 0;
		double _concluded_at = 0.0;
		double _response_concluded_at = 0.0;
		double _timeout = 0.0;
		double _resource_response_timeout = 0.0;
		double ___resource_response_timeout = 0.0;
		RequestReceipt::Callbacks _callbacks;
	friend class RequestReceipt;
	};

}
