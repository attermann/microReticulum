#pragma once

#include "Resource.h"

#include "Interface.h"
#include "Packet.h"
#include "Destination.h"
#include "Bytes.h"
#include "Type.h"
#include "Cryptography/Fernet.h"

namespace RNS {

	class ResourceData {
	public:
		ResourceData(const Link& link) : _link(link) {}
		virtual ~ResourceData() {}
	private:
		Link _link;
		Bytes _hash;
		Bytes _request_id;
		Bytes _data;
		Type::Resource::status _status = Type::Resource::NONE;
		size_t _size = 0;
		size_t _total_size = 0;
		Resource::Callbacks _callbacks;

	friend class Resource;
	};

}
