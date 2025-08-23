#pragma once

#include "Destination.h"
#include "Type.h"

#include <memory>
#include <cassert>

namespace RNS {

	class ResourceRequest;
	class ResourceResponse;
	class RequestReceipt;
	class Link;

	class LinkData;
	class RequestReceiptData;
	class Resource;
	class Packet;
	class Destination;
	class ResourceAdvertisement;
	class PacketReceipt;

	class ResourceRequest {
	public:
		double _requested_at = 0.0;
		Bytes _path_hash;
		Bytes _request_data;
	};

	class ResourceResponse {
		Bytes request_id;
		Bytes response_data;
	};

/*
	An instance of this class is returned by the ``request`` method of ``RNS.Link``
	instances. It should never be instantiated manually. It provides methods to
	check status, response time and response data when the request concludes.
*/
	class RequestReceipt {

	public:
		class Callbacks {
		public:
			using response = void(*)(const RequestReceipt& packet_receipt);
			using failed = void(*)(const RequestReceipt& packet_receipt);
			using progress = void(*)(const RequestReceipt& packet_receipt);
		public:
			response _response = nullptr;
			failed _failed = nullptr;
			progress _progress = nullptr;
		friend class RequestReceipt;
		};

	public:
		RequestReceipt(Type::NoneConstructor none) {}
		RequestReceipt(const RequestReceipt& request_receipt) : _object(request_receipt._object) {}
		//RequestReceipt(const Link& link, const PacketReceipt& packet_receipt = {Type::NONE}, const Resource& resource = {Type::NONE}, RequestReceipt::Callbacks::response response_callback = nullptr, RequestReceipt::Callbacks::failed failed_callback = nullptr, RequestReceipt::Callbacks::progress progress_callback = nullptr, double timeout = 0.0, int request_size = 0);
		RequestReceipt(const Link& link, const PacketReceipt& packet_receipt, const Resource& resource, RequestReceipt::Callbacks::response response_callback = nullptr, RequestReceipt::Callbacks::failed failed_callback = nullptr, RequestReceipt::Callbacks::progress progress_callback = nullptr, double timeout = 0.0, int request_size = 0);

		inline RequestReceipt& operator = (const RequestReceipt& packet_receipt) {
			_object = packet_receipt._object;
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const RequestReceipt& packet_receipt) const {
			return _object.get() < packet_receipt._object.get();
		}

	public:
		void request_resource_concluded(const Resource& resource);
		void __response_timeout_job();
		void request_timed_out(const PacketReceipt& packet_receipt);
		void response_resource_progress(const Resource& resource);
		void response_received(const Bytes& response);
		const Bytes& get_request_id() const;
		Type::RequestReceipt::status get_status() const;
		float get_progress() const;
		const Bytes get_response() const;
		double get_response_time() const;

		std::string toString() const;

		// getters
		const Bytes& hash() const;
		const Bytes& request_id() const;
		size_t response_transfer_size() const;

		// setters
		void response_size(size_t size);
		void response_transfer_size(size_t size);

	private:
		std::shared_ptr<RequestReceiptData> _object;

	};

/*
	This class is used to establish and manage links to other peers. When a
	link instance is created, Reticulum will attempt to establish verified
	and encrypted connectivity with the specified destination.

	:param destination: A :ref:`RNS.Destination<api-destination>` instance which to establish a link to.
	:param established_callback: An optional function or method with the signature *callback(link)* to be called when the link has been established.
	:param closed_callback: An optional function or method with the signature *callback(link)* to be called when the link is closed.
*/
	class Link {

	public:
		class Callbacks {
		public:
			using established = void(*)(Link& link);
			using closed = void(*)(Link& link);
			using packet = void(*)(const Bytes& plaintext, const Packet& packet);
			using remote_identified = void(*)(const Link& link, const Identity& remote_identity);
			using resource = void(*)(const ResourceAdvertisement& resource_advertisement);
			using resource_started = void(*)(const Resource& resource);
			using resource_concluded = void(*)(const Resource& resource);
		public:
			established _established = nullptr;
			closed _closed = nullptr;
			packet _packet = nullptr;
			remote_identified _remote_identified = nullptr;
			resource _resource = nullptr;
			resource_started _resource_started = nullptr;
			resource_concluded _resource_concluded = nullptr;
		friend class Link;
		};

	public:
		static uint8_t resource_strategies;
		static std::set<RNS::Type::Link::link_mode> ENABLED_MODES;
		static RNS::Type::Link::link_mode MODE_DEFAULT;

	public:
		Link(Type::NoneConstructor none) {
			MEM("Link NONE object created");
		}
		Link(const Link& link) : _object(link._object) {
			MEM("Link object copy created");
		}
		Link(const Destination& destination = {Type::NONE}, Callbacks::established established_callback = nullptr, Callbacks::closed closed_callback = nullptr, const Destination& owner = {Type::NONE}, const Bytes& peer_pub_bytes = {Bytes::NONE}, const Bytes& peer_sig_pub_bytes = {Bytes::NONE}, RNS::Type::Link::link_mode mode = MODE_DEFAULT);
		//Link(const Destination& destination = {Type::NONE}, Callbacks::established established_callback = nullptr, Callbacks::closed closed_callback = nullptr, const Destination& owner = {Type::NONE}, const Bytes& peer_pub_bytes = {Bytes::NONE}, const Bytes& peer_sig_pub_bytes = {Bytes::NONE}, RNS::Type::Link::link_mode mode = MODE_DEFAULT);
		virtual ~Link(){
			MEM("Link object destroyed");
		}

		Link& operator = (const Link& link) {
			_object = link._object;
			return *this;
		}
		operator bool() const {
			return _object.get() != nullptr;
		}
		bool operator < (const Link& link) const {
			return _object.get() < link._object.get();
		}

	public:
		static Bytes signalling_bytes(uint16_t mtu, RNS::Type::Link::link_mode mode);
		static uint16_t mtu_from_lr_packet(const Packet& packet);
		static uint16_t mtu_from_lp_packet(const Packet& packet);
		static uint8_t mode_byte(RNS::Type::Link::link_mode mode);
		static RNS::Type::Link::link_mode mode_from_lr_packet(const Packet& packet);
		static RNS::Type::Link::link_mode mode_from_lp_packet(const Packet& packet);
		static Bytes link_id_from_lr_packet(const Packet& packet);
		static Link validate_request( const Destination& owner, const Bytes& data, const Packet& packet);

	public:
		void load_peer(const Bytes& peer_pub_bytes, const Bytes& peer_sig_pub_bytes);
		void set_link_id(const Packet& packet);
		void handshake();
		void prove();
		void prove_packet(const Packet& packet);
		void validate_proof(const Packet& packet);
		void identify(const Identity& identity);
		const RequestReceipt request(const Bytes& path, const Bytes& data = {Bytes::NONE}, RequestReceipt::Callbacks::response response_callback = nullptr, RequestReceipt::Callbacks::failed failed_callback = nullptr, RequestReceipt::Callbacks::progress progress_callback = nullptr, double timeout = 0.0);
		void update_mdu();
		void rtt_packet(const Packet& packet);
		float get_establishment_rate();
		uint16_t get_mtu();
		uint16_t get_mdu();
		float get_expected_rate();
		RNS::Type::Link::link_mode get_mode();
		const Bytes& get_salt();
		const Bytes get_context();
		double get_age();
		double no_inbound_for();
		double no_outbound_for();
		double no_data_for();
		double inactive_for();
		const Identity& get_remote_identity();
		void had_outbound(bool is_keepalive = false);
		void teardown();
		void teardown_packet(const Packet& packet);
		void link_closed();
		void start_watchdog();
	void __watchdog_job();
		void send_keepalive();
		void handle_request(const Bytes& request_id, const ResourceRequest& unpacked_request);
		void handle_response(const Bytes& request_id, const Bytes& response_data, size_t response_size, size_t response_transfer_size);
		void request_resource_concluded(const Resource& resource);
		void response_resource_concluded(const Resource& resource);
		//z const Channel& get_channel();
		void receive(const Packet& packet);
		const Bytes encrypt(const Bytes& plaintext);
		const Bytes decrypt(const Bytes& ciphertext);
		const Bytes sign(const Bytes& message);
		bool validate(const Bytes& signature, const Bytes& message);
		void set_link_established_callback(Callbacks::established callback);
		void set_link_closed_callback(Callbacks::closed callback);
		void set_packet_callback(Callbacks::packet callback);
		void set_remote_identified_callback(Callbacks::remote_identified callback);
		void set_resource_callback(Callbacks::resource callback);
		void set_resource_started_callback(Callbacks::resource_started callback);
		void set_resource_concluded_callback(Callbacks::resource_concluded callback);
		void resource_concluded(const Resource& resource);
		void set_resource_strategy(Type::Link::resource_strategy strategy);
		void register_outgoing_resource(const Resource& resource);
		void register_incoming_resource(const Resource& resource);
		bool has_incoming_resource(const Resource& resource);
		void cancel_outgoing_resource(const Resource& resource);
		void cancel_incoming_resource(const Resource& resource);
		bool ready_for_new_resource();

		//void __str__();
		std::string toString() const;

		// getters
		const Destination& destination() const;
		// CBA LINK
		const Destination& link_destination() const;
		const Interface& attached_interface() const;
		const Bytes& link_id() const;
		const Bytes& hash() const;
		uint16_t mtu() const;
		Type::Link::status status() const;
		double establishment_timeout() const;
		uint16_t establishment_cost() const;
		double request_time() const;
		double last_inbound() const;
		std::set<RequestReceipt>& pending_requests() const;
		Type::Link::teardown_reason teardown_reason() const;
		bool initiator() const;

		// setters
		void destination(const Destination& destination);
		void attached_interface(const Interface& interface);
		void establishment_timeout(double timeout);
		void establishment_cost(uint16_t cost);
		void request_time(double time);
		void last_inbound(double time);
		void status(Type::Link::status status);

	protected:
		std::shared_ptr<LinkData> _object;

	};

}
