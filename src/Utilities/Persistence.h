#pragma once

// CBA NOTE If headers for classes referenced in this file are not included here,
//  then they MUST be included BEFORE this header is included.
#include "Transport.h"

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
			JsonArray array = dst.to<JsonArray>();
			for (T item : src)
				array.add(item);
		}
		static std::vector<T> fromJson(JsonVariantConst src) {
			std::vector<T> dst;
			for (T item : src.as<JsonArrayConst>())
				dst.push_back(item);
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
	// ArduinoJSON serialization support for RNS::Interface
	template <>
	struct Converter<RNS::Interface> {
		static bool toJson(const RNS::Interface& src, JsonVariant dst) {
			if (!src) {
				return dst.set(nullptr);
			}
			RNS::extreme("<<< Serializing interface hash " + src.get_hash().toHex());
			return dst.set(src.get_hash().toHex());
		}
		static RNS::Interface fromJson(JsonVariantConst src) {
			if (!src.isNull()) {
				RNS::Bytes hash;
				hash.assignHex(src.as<const char*>());
				RNS::extreme(">>> Deserialized interface hash " + hash.toHex());
				RNS::extreme(">>> Querying transport for interface");
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
			RNS::extreme("<<< Serializing packet hash " + src.get_hash().toHex());
			// Whenever a reference to a packet is serilized we must ensure that packet itself also gets serialized separately
			RNS::Transport::cache(src, true);
			return dst.set(src.get_hash().toHex());
		}
		static RNS::Packet fromJson(JsonVariantConst src) {
			if (!src.isNull()) {
				RNS::Bytes hash;
				hash.assignHex(src.as<const char*>());
				RNS::extreme(">>> Deserialized packet hash " + hash.toHex());
				RNS::extreme(">>> Querying transport for cached packet");
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
			RNS::extreme("<<< Serializing packet");
			dst["hash"] = src.get_hash();
			dst["raw"] = src.raw();
			dst["sent_at"] = src.sent_at();
			dst["destination_hash"] = src.get_hash();
			RNS::extreme("<<< Finished serializing packet");
			return true;
		}
		static RNS::Packet fromJson(JsonVariantConst src) {
			RNS::extreme(">>> Deserializing packet");
/*
			RNS::Packet packet;
			//packet.set_hash(src["hash"]);
			packet.sent_at(src["sent_at"]);
			RNS::Bytes destination_hash = src["destination_hash"];
*/
/**/
			RNS::Bytes raw = src["raw"];
			RNS::Packet packet(
				{RNS::Type::NONE},
				raw
			);
			//packet.set_hash(src["hash"]);
			packet.sent_at(src["sent_at"]);
			RNS::Bytes destination_hash = src["destination_hash"];
/**/
			RNS::extreme(">>> Finished deserializing packet");
			return packet;
		}
		static bool checkJson(JsonVariantConst src) {
			return
				src["hash"].is<RNS::Bytes>() &&
				src["raw"].is<RNS::Bytes>() &&
				src["sent_at"].is<double>() &&
				src["destination_hash"].is<RNS::Bytes>();
		}
	};

	// ArduinoJSON serialization support for RNS::Transport::DestinationEntry
	template <>
	struct Converter<RNS::Transport::DestinationEntry> {
		static bool toJson(const RNS::Transport::DestinationEntry& src, JsonVariant dst) {
			RNS::extreme("<<< Serializing Transport::DestinationEntry");
			dst["timestamp"] = src._timestamp;
			dst["received_from"] = src._received_from;
			dst["announce_hops"] = src._hops;
			dst["expires"] = src._expires;
			dst["random_blobs"] = src._random_blobs;
			//dst["receiving_interface"] = src._receiving_interface;
			if (src._receiving_interface) {
				dst["receiving_interface_hash"] = src._receiving_interface.get_hash();
			}
			else {
				dst["receiving_interface_hash"] = nullptr;
			}
			//dst["packet"] = src._announce_packet;
			if (src._announce_packet) {
				dst["packet_hash"] = src._announce_packet.get_hash();
				// Whenever a reference to a packet is serilized we must ensure that packet itself also gets serialized separately
				RNS::Transport::cache(src._announce_packet, true);
			}
			else {
				dst["packet_hash"] = nullptr;
			}
			RNS::extreme("<<< Finished Serializing Transport::DestinationEntry");
			return true;
		}
		static RNS::Transport::DestinationEntry fromJson(JsonVariantConst src) {
			RNS::extreme(">>> Deserializing Transport::DestinationEntry");
/**/
			RNS::Transport::DestinationEntry dst;
			dst._timestamp = src["timestamp"];
			dst._received_from = src["received_from"];
			dst._hops = src["announce_hops"];
			dst._expires = src["expires"];
			dst._random_blobs = src["random_blobs"].as<std::set<RNS::Bytes>>();
			//dst._receiving_interface = src["receiving_interface"];
			RNS::Bytes interface_hash = src["receiving_interface_hash"];
			if (interface_hash)
				// Query transport for matching interface
				dst._receiving_interface = RNS::Transport::find_interface_from_hash(interface_hash);
			//dst._announce_packet = src["packet"];
			RNS::Bytes packet_hash = src["packet_hash"];
			if (packet_hash)
				// Query transport for matching packet
				dst._announce_packet = RNS::Transport::get_cached_packet(packet_hash);
/**/
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
			RNS::extreme(">>> Finished Deserializing Transport::DestinationEntry");
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			return
				src["timestamp"].is<double>() &&
				src["received_from"].is<RNS::Bytes>() &&
				src["announce_hops"].is<int>() &&
				src["expires"].is<double>() &&
				src["random_blobs"].is<std::set<RNS::Bytes>>() &&
				src["receiving_interface_hash"].is<RNS::Bytes>() &&
				src["packet_hash"].is<RNS::Bytes>();
		}
	};

}

namespace RNS { namespace Persistence {

	template <typename T> bool serialize(const T& obj, const char* file_path, size_t max_size) {
		DynamicJsonDocument doc(1024);
		doc.set(obj);
		RNS::Bytes data;
		size_t length = serializeJson(doc, data.writable(max_size), max_size);
		//size_t length = serializeMsgPack(doc, data.writable(max_size), max_size);
		if (length < max_size) {
			data.resize(length);
		}
		RNS::extreme("Persistence::serialize: serialized " + std::to_string(length) + " bytes");
		if (length > 0) {
			if (RNS::Utilities::OS::write_file(data, file_path)) {
				RNS::extreme("Persistence::serialize: wrote: " + std::to_string(data.size()) + " bytes");
			}
			else {
				RNS::extreme("Persistence::serialize: write failed");
				return false;
			}
		}
		else {
			RNS::extreme("Persistence::serialize: failed to serialize");
			return false;
		}
		return true;
	}
	template <typename T> bool deserialize(T& obj, const char* file_path) {
		DynamicJsonDocument doc(1024);
		RNS::Bytes data = RNS::Utilities::OS::read_file(file_path);
		if (data) {
			RNS::extreme("Persistence::deserialize: read: " + std::to_string(data.size()) + " bytes");
			//RNS::extreme("testDeserializeVector: data: " + data.toString());
			DeserializationError error = deserializeJson(doc, data.data());
			//DeserializationError error = deserializeMsgPack(doc, data.data());
			if (!error) {
				RNS::extreme("Persistence::deserialize: successfully deserialized");
				obj = doc.as<T>();
				return true;
			}
			else {
				RNS::extreme("Persistence::deserialize: failed to deserialize");
			}
		}
		else {
			RNS::extreme("Persistence::deserialize: read failed");
		}
		return false;;
	}

} }
