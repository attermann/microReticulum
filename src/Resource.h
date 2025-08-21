#pragma once

#include "Destination.h"
#include "Type.h"

#include <memory>
#include <cassert>

namespace RNS {

	class ResourceData;
	class Packet;
	class Destination;
	class Link;
	class Resource;

	class Resource {

	public:
		class Callbacks {
		public:
			//using concluded = void(*)(const Resource& resource);
			typedef std::function<void(const Resource& resource)> concluded;
			using progress = void(*)(const Resource& resource);
		public:
			concluded _concluded = nullptr;
			progress _progress = nullptr;
		friend class Resource;
		};

	public:
		Resource(Type::NoneConstructor none) {
			MEM("Resource NONE object created");
		}
		Resource(const Resource& resource) : _object(resource._object) {
			MEM("Resource object copy created");
		}
		//Resource(const Link& link = {Type::NONE});
		Resource(const Bytes& data, const Link& link, const Bytes& request_id, bool is_response, double timeout);
		Resource(const Bytes& data, const Link& link, bool advertise = true, bool auto_compress = true, Callbacks::concluded callback = nullptr, Callbacks::progress progress_callback = nullptr, double timeout = 0.0, int segment_index = 1, const Bytes& original_hash = {Type::NONE}, const Bytes& request_id = {Type::NONE}, bool is_response = false);
		virtual ~Resource(){
			MEM("Resource object destroyed");
		}

		Resource& operator = (const Resource& resource) {
			_object = resource._object;
			return *this;
		}
		operator bool() const {
			return _object.get() != nullptr;
		}
		bool operator < (const Resource& resource) const {
			return _object.get() < resource._object.get();
			//return _object->_hash < resource._object->_hash;
		}

	public:
	    //p static def accept(advertisement_packet, callback=None, progress_callback = None, request_id = None):

	public:
//p def hashmap_update_packet(self, plaintext):
//p def hashmap_update(self, segment, hashmap):
//p def get_map_hash(self, data):
//p def advertise(self):
//p def __advertise_job(self):
//p def watchdog_job(self):
//p def __watchdog_job(self):
//p def assemble(self):
//p def prove(self):
		void validate_proof(const Bytes& proof_data);
//p def receive_part(self, packet):
//p def request_next(self):
//p def request(self, request_data):
		void cancel();
//p def set_callback(self, callback):
//p def progress_callback(self, callback):
		float get_progress() const;
//p def get_transfer_size(self):
//p def get_data_size(self):
//p def get_parts(self):
//p def get_segments(self):
//p def get_hash(self):
//p def is_compressed(self):
		void set_concluded_callback(Callbacks::concluded callback);
		void set_progress_callback(Callbacks::progress callback);

		std::string toString() const;

		// getters
		const Bytes& hash() const;
		const Bytes& request_id() const;
		const Bytes& data() const;
		const Type::Resource::status status() const;
		const size_t size() const;
		const size_t total_size() const;

		// setters

	protected:
		std::shared_ptr<ResourceData> _object;

	};


	class ResourceAdvertisement {

	};

}
