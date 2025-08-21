#pragma once

// CBA NOTE If headers for classes referenced in this file are not included here,
//  then they MUST be included BEFORE this header is included.
#include "Transport.h"
#include "Type.h"

#include <ArduinoJson.h>

#include <map>
#include <vector>
#include <set>
#include <string>

namespace ArduinoJson {

	// ArduinoJSON serialization support for std::vector<T>
	template <typename T>
	struct Converter<std::vector<T>> {
		static void toJson(const std::vector<T>& src, JsonVariant dst) {
			TRACE("<<< Serializing vector");
			JsonArray array = dst.to<JsonArray>();
			for (T item : src)
				array.add(item);
			TRACE("<<< Finished serializing vector");
		}
		static std::vector<T> fromJson(JsonVariantConst src) {
			TRACE("<<< Deserializing vector");
			std::vector<T> dst;
			for (T item : src.as<JsonArrayConst>())
				dst.push_back(item);
			TRACE("<<< Finished deserializing vector");
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			JsonArrayConst array = src;
			bool result = array;
			for (JsonVariantConst item : array)
				result &= item.is<T>();
			return result;
		}
	};

	// ArduinoJSON serialization support for std::set<T>
	template <typename T>
	struct Converter<std::set<T>> {
		static void toJson(const std::set<T>& src, JsonVariant dst) {
			JsonArray array = dst.to<JsonArray>();
			for (T item : src)
				array.add(item);
		}
		static std::set<T> fromJson(JsonVariantConst src) {
			std::set<T> dst;
			for (T item : src.as<JsonArrayConst>())
				dst.insert(item);
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			JsonArrayConst array = src;
			bool result = array;
			for (JsonVariantConst item : array)
				result &= item.is<T>();
			return result;
		}
	};

	// ArduinoJSON serialization support for std::map<std::string, T>
	template <typename T>
	struct Converter<std::map<std::string, T>> {
		static void toJson(const std::map<std::string, T>& src, JsonVariant dst) {
			JsonObject obj = dst.to<JsonObject>();
			for (const auto& item : src)
				obj[item.first] = item.second;
		}
		static std::map<std::string, T> fromJson(JsonVariantConst src) {
			std::map<std::string, T> dst;
			for (JsonPairConst item : src.as<JsonObjectConst>())
				dst[item.key().c_str()] = item.value().as<T>();
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			JsonObjectConst obj = src;
			bool result = obj;
			for (JsonPairConst item : obj)
				result &= item.value().is<T>();
			return result;
		}
	};

	// ArduinoJSON serialization support for std::map<Bytes, T>
	template <typename T>
	struct Converter<std::map<RNS::Bytes, T>> {
		static void toJson(const std::map<RNS::Bytes, T>& src, JsonVariant dst) {
			JsonObject obj = dst.to<JsonObject>();
			for (const auto& item : src) {
				//obj[item.first] = item.second;
				obj[item.first.toHex()] = item.second;
			}
		}
		static std::map<RNS::Bytes, T> fromJson(JsonVariantConst src) {
			std::map<RNS::Bytes, T> dst;
			for (JsonPairConst item : src.as<JsonObjectConst>()) {
				//dst[item.key().c_str()] = item.value().as<T>();
				RNS::Bytes key;
				key.assignHex(item.key().c_str());
				//dst[key] = item.value().as<T>();
				dst.insert({key, item.value().as<T>()});
			}
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			JsonObjectConst obj = src;
			bool result = obj;
			for (JsonPairConst item : obj) {
				result &= item.value().is<T>();
			}
			return result;
		}
	};

/*
	// ArduinoJSON serialization support for RNS::Bytes
	template <>
	struct Converter<RNS::Bytes> {
		static bool toJson(const RNS::Bytes& src, JsonVariant dst) {
			//TRACE("<<< Serializing Bytes");
			//TRACE("<<< Finished serializing Bytes");
			return true;
		}
		static RNS::Bytes fromJson(JsonVariantConst src) {
			Bytes bytes;
			//TRACE(">>> Deserializing Bytes");
			//TRACE(">>> Finished deserializing Bytes");
			return bytes;
		}
		static bool checkJson(JsonVariantConst src) {
			return
				src.is<RNS::Bytes>();
		}
	};
*/

/*
	// ArduinoJSON serialization support for RNS::Interface
	template <>
	struct Converter<RNS::Interface> {
		static bool toJson(const RNS::Interface& src, JsonVariant dst) {
			if (!src) {
				return dst.set(nullptr);
			}
			//TRACE("<<< Serializing interface hash " + src.get_hash().toHex());
			return dst.set(src.get_hash().toHex());
		}
		static RNS::Interface fromJson(JsonVariantConst src) {
			if (!src.isNull()) {
				RNS::Bytes hash;
				hash.assignHex(src.as<const char*>());
				//TRACE(">>> Deserialized interface hash " + hash.toHex());
				//TRACE(">>> Querying transport for interface");
				// Query transport for matching interface
				return RNS::Transport::find_interface_from_hash(hash);
			}
			else {
				return {RNS::Type::NONE};
			}
		}
		static bool checkJson(JsonVariantConst src) {
			return src.is<const char*>() && strlen(src.as<const char*>()) == 64;
		}
	};
*/

/*
	// ArduinoJSON serialization support for RNS::Packet
	template <>
	struct Converter<RNS::Packet> {
		static bool toJson(const RNS::Packet& src, JsonVariant dst) {
			if (!src) {
				return dst.set(nullptr);
			}
			//TRACE("<<< Serializing packet hash " + src.get_hash().toHex());
			// Whenever a reference to a packet is serialized we must ensure that packet itself also gets serialized separately
			RNS::Transport::cache_packet(src, true);
			return dst.set(src.get_hash().toHex());
		}
		static RNS::Packet fromJson(JsonVariantConst src) {
			if (!src.isNull()) {
				RNS::Bytes hash;
				hash.assignHex(src.as<const char*>());
				//TRACE(">>> Deserialized packet hash " + hash.toHex());
				//TRACE(">>> Querying transport for cached packet");
				// Query transport for matching packet
				return RNS::Transport::get_cached_packet(hash);
			}
			else {
				return {RNS::Type::NONE};
			}
		}
		static bool checkJson(JsonVariantConst src) {
			return src.is<const char*>() && strlen(src.as<const char*>()) == 64;
		}
	};
*/
	// ArduinoJSON serialization support for RNS::Packet
	template <>
	struct Converter<RNS::Packet> {
		static bool toJson(const RNS::Packet& src, JsonVariant dst) {
			//TRACE("<<< Serializing Packet");
			//dst["hash"] = src.get_hash();
			dst["raw"] = src.raw();
			dst["sent_at"] = src.sent_at();
			dst["destination_hash"] = src.get_hash();
			//TRACE("<<< Finished serializing Packet");
			return true;
		}
		static RNS::Packet fromJson(JsonVariantConst src) {
			//TRACE(">>> Deserializing Packet");
			RNS::Bytes raw = src["raw"];
			//RNS::Bytes raw = src["raw"].as<const RNS::Bytes&>();
			RNS::Packet packet(RNS::Destination(RNS::Type::NONE), raw);
			//packet.set_hash(src["hash"]);
			packet.sent_at(src["sent_at"]);
			RNS::Bytes destination_hash = src["destination_hash"];
			// set cached flag since pcket was read from cache
			packet.cached(true);
			//TRACE(">>> Finished deserializing Packet");
			return packet;
		}
		static bool checkJson(JsonVariantConst src) {
			return
				//src["hash"].is<RNS::Bytes>() &&
				src["raw"].is<RNS::Bytes>() &&
				src["sent_at"].is<double>() &&
				src["destination_hash"].is<RNS::Bytes>();
		}
	};

	// ArduinoJSON serialization support for RNS::Transport::PacketEntry
	template <>
	struct Converter<RNS::Transport::PacketEntry> {
		static bool toJson(const RNS::Transport::PacketEntry& src, JsonVariant dst) {
			//TRACE("<<< Serializing Transport::PacketEntry");
			//dst["hash"] = src._hash;
			dst["raw"] = src._raw;
			dst["sent_at"] = src._sent_at;
			dst["destination_hash"] = src._destination_hash;
			//TRACE("<<< Finished serializing Transport::PacketEntry");
			return true;
		}
		static RNS::Transport::PacketEntry fromJson(JsonVariantConst src) {
			//TRACE(">>> Deserializing Transport::PacketEntry");
			RNS::Transport::PacketEntry dst;
			//dst._hash = src["hash"];
			dst._raw = src["raw"];
			dst._sent_at = src["sent_at"];
			dst._destination_hash = src["destination_hash"];
			// set cached flag since pcket was read from cache
			dst._cached = true;
			//TRACE(">>> Finished deserializing Transport::PacketEntry");
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			return
				//src["hash"].is<RNS::Bytes>() &&
				src["raw"].is<RNS::Bytes>() &&
				src["sent_at"].is<double>() &&
				src["destination_hash"].is<RNS::Bytes>();
		}
	};

#if 1
	// ArduinoJSON serialization support for RNS::Transport::DestinationEntry
	template <>
	struct Converter<RNS::Transport::DestinationEntry> {
		static bool toJson(const RNS::Transport::DestinationEntry& src, JsonVariant dst) {
			//TRACE("<<< Serializing Transport::DestinationEntry");
			dst["timestamp"] = src._timestamp;
			dst["received_from"] = src._received_from;
			dst["announce_hops"] = src._hops;
			dst["expires"] = src._expires;
			dst["random_blobs"] = src._random_blobs;
/*
			//dst["interface_hash"] = src._receiving_interface;
			if (src._receiving_interface) {
				dst["interface_hash"] = src._receiving_interface.get_hash();
			}
			else {
				dst["interface_hash"] = nullptr;
			}
			// CBA TODO Move packet serialization to *after* destination table serialization since packets are useless
			//  anyway if there's no space to left to write destination table.
			// Whenever a reference to a packet is serialized we must ensure that packet itself also gets serialized separately
			//dst["packet"] = src._announce_packet;
			if (src._announce_packet) {
				dst["packet_hash"] = src._announce_packet.get_hash();
				// Only cache packet if not already cached
				if (!src._announce_packet.cached()) {
					if (RNS::Transport::cache_packet(src._announce_packet, true)) {
						const_cast<RNS::Packet&>(src._announce_packet).cached(true);
					}
				}
				else {
					TRACE("Destination announce packet " + src._announce_packet.get_hash().toHex() + " is already cached");
				}
			}
			else {
				dst["packet_hash"] = nullptr;
			}
*/
			dst["interface_hash"] = src._receiving_interface;
			dst["packet_hash"] = src._announce_packet;
			//TRACE("<<< Finished Serializing Transport::DestinationEntry");
			return true;
		}
		static RNS::Transport::DestinationEntry fromJson(JsonVariantConst src) {
			//TRACE(">>> Deserializing Transport::DestinationEntry");
			RNS::Transport::DestinationEntry dst;
			dst._timestamp = src["timestamp"];
			dst._received_from = src["received_from"];
			dst._hops = src["announce_hops"];
			dst._expires = src["expires"];
			dst._random_blobs = src["random_blobs"].as<std::set<RNS::Bytes>>();
/*
			//dst._receiving_interface = src["interface_hash"];
			RNS::Bytes interface_hash = src["interface_hash"];
			if (interface_hash) {
				// Query transport for matching interface
				dst._receiving_interface = RNS::Transport::find_interface_from_hash(interface_hash);
			}
			//dst._announce_packet = src["packet"];
			RNS::Bytes packet_hash = src["packet_hash"];
			if (packet_hash) {
				// Query transport for matching packet
				dst._announce_packet = RNS::Transport::get_cached_packet(packet_hash);
			}
*/
			dst._receiving_interface = src["interface_hash"];
			dst._announce_packet = src["packet_hash"];
/*
			//RNS::Transport::DestinationEntry dst(src["timestamp"], src["received_from"], src["announce_hops"], src["expires"], src["random_blobs"], src["receiving_interface"], src["packet"]);
			RNS::Transport::DestinationEntry dst(
				src["timestamp"].as<double>(),
				src["received_from"].as<RNS::Bytes>(),
				src["announce_hops"].as<int>(),
				src["expires"].as<double>(),
				src["random_blobs"].as<std::set<RNS::Bytes>>(),
				src["receiving_interface"].as<RNS::Interface>(),
				src["packet"].as<RNS::Packet>()
			);
*/
			//TRACE(">>> Finished Deserializing Transport::DestinationEntry");
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			return
				src["timestamp"].is<double>() &&
				src["received_from"].is<RNS::Bytes>() &&
				src["announce_hops"].is<int>() &&
				src["expires"].is<double>() &&
				src["random_blobs"].is<std::set<RNS::Bytes>>() &&
				src["interface_hash"].is<RNS::Bytes>() &&
				src["packet_hash"].is<RNS::Bytes>();
		}
	};
#endif

}

#if 0
namespace RNS {

	inline bool convertToJson(const Transport::DestinationEntry& src, JsonVariant dst) {
		//TRACE("<<< NEW Serializing Transport::DestinationEntry");
		dst["timestamp"] = src._timestamp;
		dst["received_from"] = src._received_from;
		dst["announce_hops"] = src._hops;
		dst["expires"] = src._expires;
		dst["random_blobs"] = src._random_blobs;
/*
		//dst["interface_hash"] = src._receiving_interface;
		if (src._receiving_interface) {
			dst["interface_hash"] = src._receiving_interface.get_hash();
		}
		else {
			dst["interface_hash"] = nullptr;
		}
		// CBA TODO Move packet serialization to *after* destination table serialization since packets are useless
		//  anyway if there's no space to left to write destination table.
		// Whenever a reference to a packet is serialized we must ensure that packet itself also gets serialized separately
		//dst["packet"] = src._announce_packet;
		if (src._announce_packet) {
			dst["packet_hash"] = src._announce_packet.get_hash();
			// Only cache packet if not already cached
			if (!src._announce_packet.cached()) {
				if (RNS::Transport::cache_packet(src._announce_packet, true)) {
					const_cast<RNS::Packet&>(src._announce_packet).cached(true);
				}
			}
			else {
				TRACE("Destination announce packet " + src._announce_packet.get_hash().toHex() + " is already cached");
			}
		}
		else {
			dst["packet_hash"] = nullptr;
		}
*/
		dst["interface_hash"] = src._receiving_interface;
		dst["packet_hash"] = src._announce_packet;
		//TRACE("<<< Finished Serializing Transport::DestinationEntry");
		return true;
	}

	inline void convertFromJson(JsonVariantConst src, Transport::DestinationEntry& dst) {
		//TRACE(">>> NEW Deserializing Transport::DestinationEntry");
		dst._timestamp = src["timestamp"];
		dst._received_from = src["received_from"];
		dst._hops = src["announce_hops"];
		dst._expires = src["expires"];
		dst._random_blobs = src["random_blobs"].as<std::set<RNS::Bytes>>();
/*
		//dst._receiving_interface = src["interface_hash"];
		RNS::Bytes interface_hash = src["interface_hash"];
		if (interface_hash) {
			// Query transport for matching interface
			dst._receiving_interface = RNS::Transport::find_interface_from_hash(interface_hash);
		}
		//dst._announce_packet = src["packet"];
		RNS::Bytes packet_hash = src["packet_hash"];
		if (packet_hash) {
			// Query transport for matching packet
			dst._announce_packet = RNS::Transport::get_cached_packet(packet_hash);
		}
*/
		dst._receiving_interface = src["interface_hash"];
		dst._announce_packet = src["packet_hash"];
/*
		//RNS::Transport::DestinationEntry dst(src["timestamp"], src["received_from"], src["announce_hops"], src["expires"], src["random_blobs"], src["receiving_interface"], src["packet"]);
		RNS::Transport::DestinationEntry dst(
			src["timestamp"].as<double>(),
			src["received_from"].as<RNS::Bytes>(),
			src["announce_hops"].as<int>(),
			src["expires"].as<double>(),
			src["random_blobs"].as<std::set<RNS::Bytes>>(),
			src["receiving_interface"].as<RNS::Interface>(),
			src["packet"].as<RNS::Packet>()
		);
*/
		//TRACE(">>> Finished Deserializing Transport::DestinationEntry");
	}

	inline bool canConvertFromJson(JsonVariantConst src, const Transport::DestinationEntry&) {
		TRACE("=== NEW Checking Transport::DestinationEntry");
		return
			src["timestamp"].is<double>() &&
			src["received_from"].is<RNS::Bytes>() &&
			src["announce_hops"].is<int>() &&
			src["expires"].is<double>() &&
			src["random_blobs"].is<std::set<RNS::Bytes>>() &&
			src["interface_hash"].is<RNS::Bytes>() &&
			src["packet_hash"].is<RNS::Bytes>();
	}
}
#endif

namespace RNS { namespace Persistence {

	static DynamicJsonDocument _document(Type::Persistence::DOCUMENT_MAXSIZE);
	static Bytes _buffer(Type::Persistence::BUFFER_MAXSIZE);

	template <typename T> size_t crc(const T& obj) {
		TRACE("Persistence::crc<T>");
		_document.set(obj);
		size_t size = _buffer.capacity();
#ifdef USE_MSGPACK
		size_t length = serializeMsgPack(_document, _buffer.writable(size), size);
#else
		size_t length = serializeJson(_document, _buffer.writable(size), size);
#endif
		if (length < size) {
			_buffer.resize(length);
		}
		TRACEF("Persistence::crc: serialized %d bytes", length);
		return Utilities::Crc::crc32(0, _buffer.data(), _buffer.size());
	}

	template <typename T> size_t serialize(const T& obj, const char* file_path) {
		TRACE("Persistence::serialize<T>");
		_document.set(obj);
		size_t size = _buffer.capacity();
#ifdef USE_MSGPACK
		size_t length = serializeMsgPack(_document, _buffer.writable(size), size);
#else
		size_t length = serializeJson(_document, _buffer.writable(size), size);
#endif
		if (length < size) {
			_buffer.resize(length);
		}
		TRACEF("Persistence::serialize: serialized %d bytes", length);
		size_t wrote = 0;
		if (length > 0) {
			wrote = RNS::Utilities::OS::write_file(file_path, _buffer);
			if (wrote == _buffer.size()) {
				TRACEF("Persistence::serialize: wrote %d bytes", _buffer.size());
			}
			else {
				TRACE("Persistence::serialize: write failed");
				return 0;
			}
		}
		else {
			TRACE("Persistence::serialize: failed to serialize");
			return 0;
		}
		return wrote;
	}

	template <typename T> size_t deserialize(T& obj, const char* file_path) {
		TRACE("Persistence::deserialize<T>");
		size_t read = RNS::Utilities::OS::read_file(file_path, _buffer);
		if (read > 0) {
			TRACEF("Persistence::deserialize: read: %d bytes", _buffer.size());
			//TRACE("testDeserializeVector: data: " + _buffer.toString());
#ifdef USE_MSGPACK
			DeserializationError error = deserializeMsgPack(_document, _buffer.data());
#else
			DeserializationError error = deserializeJson(_document, _buffer.data());
#endif
			if (!error) {
				TRACE("Persistence::deserialize: successfully deserialized document");
				obj = _document.as<T>();
				// CBA Following obj check doesn't work when T is a collection
				//if (obj) {
					return read;
				//}
				TRACE("Persistence::deserialize: failed to compose object");
			}
			else {
				TRACE("Persistence::deserialize: failed to deserialize");
			}
		}
		else {
			TRACE("Persistence::deserialize: read failed");
		}
		return 0;
	}

#if 1
	template <typename T> size_t crc(std::map<Bytes, T>& map) {
		TRACE("Persistence::crc<map<Bytes, T>>");

		uint32_t crc = 0;
		crc = Utilities::Crc::crc32(crc, '{');
		for (const auto& [key, value] : map) {
			crc = Utilities::Crc::crc32(crc, '"');
			std::string hex = key.toHex();
			crc = Utilities::Crc::crc32(crc, hex.c_str());
			crc = Utilities::Crc::crc32(crc, '"');
			crc = Utilities::Crc::crc32(crc, ':');

			_document.set(value);
			size_t size = _buffer.capacity();
#ifdef USE_MSGPACK
			size_t length = serializeMsgPack(_document, _buffer.writable(size), size);
#else
			size_t length = serializeJson(_document, _buffer.writable(size), size);
#endif
			if (length < size) {
				_buffer.resize(length);
			}
			TRACEF("Persistence::crc: serialized entry %d bytes", length);

			if (length > 0) {
				crc = Utilities::Crc::crc32(crc, _buffer.data(), _buffer.size());
			}
			else {
				// if failed to serialize entry then write empty entry
				crc = Utilities::Crc::crc32(crc, "{}");
			}
			crc = Utilities::Crc::crc32(crc, ',');
		}
		return Utilities::Crc::crc32(crc, '}');
	}

	template <typename T> size_t serialize(std::map<Bytes, T>& map, const char* file_path, uint32_t& crc) {
		TRACE("Persistence::serialize<map<Bytes,T>>");

		// CBA TODO: Use stream here instead to avoid having to buffer entire structure
		RNS::FileStream stream = RNS::Utilities::OS::open_file(file_path, RNS::FileStream::MODE_WRITE);
		if (!stream) {
			TRACE("Persistence::serialize: failed to open write stream");
			return 0;
		}

		stream.write('{');
		for (const auto& [key, value] : map) {
			stream.write('"');
			std::string hex = key.toHex();
			stream.write(hex.c_str());
			stream.write('"');
			stream.write(':');

			_document.set(value);
#ifdef USE_MSGPACK
			size_t length = serializeMsgPack(_document, stream);
#else
			size_t length = serializeJson(_document, stream);
#endif
			TRACEF("Persistence::serialize: serialized entry %d bytes", length);

			if (length == 0) {
				// if failed to serialize entry then write empty entry
				stream.write("{}");
			}
			stream.write(',');
		}
		stream.write('}');
		TRACEF("Persistence::serialize: stream size: %d bytes", stream.size());
		crc = stream.crc();
		return stream.size();
	}

	template <typename T> size_t serialize(std::map<Bytes, T>& map, const char* file_path) {
		uint32_t crc;
		return serialize(map, file_path, crc);
	}

	template <typename T> size_t deserialize(std::map<Bytes, T>& map, const char* file_path, uint32_t& crc) {
		TRACE("Persistence::deserialize<map<Bytes,T>>");

		// CBA TODO: Use stream here instead to avoid having to buffer entire structure
		RNS::FileStream stream = RNS::Utilities::OS::open_file(file_path, RNS::FileStream::MODE_READ);
		if (!stream) {
			TRACE("Persistence::deserialize: failed to open read stream");
			return 0;
		}
		TRACEF("Persistence::deserialize: size: %d bytes", stream.size());

		map.clear();

		// find opening brace
		if (stream.find('{')) {
			char key_str[RNS::Type::Reticulum::DESTINATION_LENGTH*2+1] = "";
			do {
				key_str[0] = 0;
				// find map key opening quote
				if (stream.find('"')) {
					if (stream.readBytesUntil('"', key_str, sizeof(key_str)) > 0) {
						Bytes key;
						key.assignHex(key_str);
						TRACEF("Persistence::deserialize: key: %s", key.toHex().c_str());
						if (stream.find(':')) {
#ifdef USE_MSGPACK
							//DeserializationError error = deserializeMsgPack(_document, _buffer.data());
							DeserializationError error = deserializeMsgPack(_document, stream);
#else
							//DeserializationError error = deserializeJson(_document, _buffer.data());
							DeserializationError error = deserializeJson(_document, stream);
#endif
							if (!error) {
								TRACE("Persistence::deserialize: successfully deserialized entry");
								T obj = _document.as<T>();
								// CBA Following obj check doesn't work when T is a collection
								//if (obj) {
								//	TRACE("Persistence::deserialize: failed to compose object");
								//	break;
								//}
								map.insert({key, obj});
							}
							else {
								TRACE("Persistence::deserialize: failed to deserialize entry");
								//break;
							}

							if (!stream.find(',')) {
								break;
							}

						}
					}
				}
			} while (key_str[0] != 0);
		}
		crc = stream.crc();
		return stream.size();
	}

	template <typename T> size_t deserialize(std::map<Bytes, T>& map, const char* file_path) {
		uint32_t crc;
		return deserialize(map, file_path, crc);
	}
#endif

} }
