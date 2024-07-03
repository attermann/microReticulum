#include "Bytes.h"

using namespace RNS;

// Creates new shared data for instance
// - If size is specified (>0) then create shared data with initial size
// - If size is not specified (<=0) then create empty shared data
void Bytes::newData(size_t size /*= 0*/) {
	MEM("Bytes is creating its own data");
	if (size > 0) {
		// CBA Note that this method of construction creates vector with specified and initializes all eleemtnets with default values
		Data* data = new Data(size);
		if (data == nullptr) {
			ERROR("Bytes failed to allocate data buffer");
			throw std::runtime_error("Failed to allocate data buffer");
		}
		_data = SharedData(data);
	}
	else {
		Data* data = new Data();
		if (data == nullptr) {
			ERROR("Bytes failed to allocate empty data buffer");
			throw std::runtime_error("Failed to allocate empty data buffer");
		}
		_data = SharedData(data);
	}
	_exclusive = true;
}

// Ensures that instance has exclusive shared data
// - If instance has no shared data then create new shared data
// - If instance does not have exclusive on shared data that is not empty then make a copy of shared data (if requests) and resize (if requested)
// - If instance does not have exclusive on shared data that is empty then then create new shared data
// - If instance already has exclusive on shared data then do nothing except resize (if requested)
void Bytes::exclusiveData(bool copy /*= true*/, size_t size /*= 0*/) {
	if (!_data) {
		newData(size);
	}
	else if (!_exclusive) {
		Data *data;
		if (copy && !_data->empty()) {
			//TRACE("Bytes is creating a writable copy of its shared data");
			data = new Data(*_data.get());
			if (data == nullptr) {
				ERROR("Bytes failed to duplicate data buffer");
				throw std::runtime_error("Failed to duplicate data buffer");
			}
			if (size > 0) {
				data->resize(size);
			}
			_data = SharedData(data);
			_exclusive = true;
		}
		else {
			//TRACE("Bytes is creating its own data because shared is empty");
			//data = new Data();
			//if (data == nullptr) {
			//	ERROR("Bytes failed to allocate empty data buffer");
			//	throw std::runtime_error("Failed to allocate empty data buffer");
			//}
			//_data = SharedData(data);
			//_exclusive = true;
			newData(size);
		}
	}
	else if (size > 0) {
		_data->resize(size);
	}
}

int8_t Bytes::compare(const Bytes& bytes) const {
	if (_data == bytes._data) {
		return 0;
	}
	else if (!_data) {
		return -1;
	}
	else if (!bytes._data) {
		return 1;
	}
	else if (*_data < *(bytes._data)) {
		return -1;
	}
	else if (*_data > *(bytes._data)) {
		return 1;
	}
	else {
		return 0;
	}
}

void Bytes::assignHex(const char* hex) {
	// if assignment is empty then clear data and don't bother creating new
	if (hex == nullptr || hex[0] == 0) {
		_data = nullptr;
		_exclusive = true;
		return;
	}
	newData();
	size_t len = strlen(hex);
	for (size_t i = 0; i < len; i += 2) {
		uint8_t byte = (hex[i] % 32 + 9) % 25 * 16 + (hex[i+1] % 32 + 9) % 25;
		_data->push_back(byte);
	}
}

void Bytes::appendHex(const char* hex) {
	// if append is empty then do nothing
	if (hex == nullptr || hex[0] == 0) {
		return;
	}
	exclusiveData();
	size_t len = strlen(hex);
	for (size_t i = 0; i < len; i += 2) {
		uint8_t byte = (hex[i] % 32 + 9) % 25 * 16 + (hex[i+1] % 32 + 9) % 25;
		_data->push_back(byte);
	}
}

const char hex_upper_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
const char hex_lower_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

std::string RNS::hexFromByte(uint8_t byte, bool upper /*= true*/) {
	std::string hex;
	if (upper) {
		hex += hex_upper_chars[ (byte&  0xF0) >> 4];
		hex += hex_upper_chars[ (byte&  0x0F) >> 0];
	}
	else {
		hex += hex_lower_chars[ (byte&  0xF0) >> 4];
		hex += hex_lower_chars[ (byte&  0x0F) >> 0];
	}
	return hex;
}

std::string Bytes::toHex(bool upper /*= false*/) const {
	if (!_data) {
		return "";
	}
	std::string hex;
	for (uint8_t byte : *_data) {
		if (upper) {
			hex += hex_upper_chars[ (byte&  0xF0) >> 4];
			hex += hex_upper_chars[ (byte&  0x0F) >> 0];
		}
		else {
			hex += hex_lower_chars[ (byte&  0xF0) >> 4];
			hex += hex_lower_chars[ (byte&  0x0F) >> 0];
		}
	}
	return hex;
}

// mid
Bytes Bytes::mid(size_t beginpos, size_t len) const {
	if (!_data || beginpos >= size()) {
		return NONE;
	}
	if ((beginpos + len) >= size()) {
		len = (size() - beginpos);
	 }
	 return {data() + beginpos, len};
}

// to end
Bytes Bytes::mid(size_t beginpos) const {
	if (!_data || beginpos >= size()) {
		return NONE;
	}
	 return {data() + beginpos, size() - beginpos};
}
