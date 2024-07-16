#include "Bytes.h"

using namespace RNS;

// Creates new shared data for instance
// - If capacity is specified (>0) then create empty shared data with initial reserved capacity
// - If capacity is not specified (<=0) then create empty shared data with no initial capacity
void Bytes::newData(size_t capacity /*= 0*/) {
	MEMF("Bytes is creating own data with capacity %u", capacity);
MEM("newData: Creating new data...");
	Data* data = new Data();
	if (data == nullptr) {
		ERROR("Bytes failed to allocate empty data buffer");
		throw std::runtime_error("Failed to allocate empty data buffer");
	}
MEM("newData: Created new data");
	if (capacity > 0) {
MEM("newData: Reserving data capacity...");
		data->reserve(capacity);
MEM("newData: Reserved data capacity");
	}
MEM("newData: Assigning data to shared data pointer...");
	_data = SharedData(data);
MEM("newData: Assigned data to shared data pointer");
	_exclusive = true;
}

// Ensures that instance has exclusive shared data
// - If instance has no shared data then create new shared data
// - If instance does not have exclusive on shared data that is not empty then make a copy of shared data (if requests) and reserve capacity (if requested)
// - If instance does not have exclusive on shared data that is empty then then create new shared data
// - If instance already has exclusive on shared data then do nothing except reserve capacity (if requested)
void Bytes::exclusiveData(bool copy /*= true*/, size_t capacity /*= 0*/) {
	if (!_data) {
		newData(capacity);
	}
	else if (!_exclusive) {
		if (copy && !_data->empty()) {
			//TRACE("Bytes is creating a writable copy of its shared data");
			//Data* data = new Data(*_data.get());
MEM("exclusiveData: Creating new data...");
			Data* data = new Data();
			if (data == nullptr) {
				ERROR("Bytes failed to duplicate data buffer");
				throw std::runtime_error("Failed to duplicate data buffer");
			}
MEM("exclusiveData: Created new data");
			if (capacity > 0) {
MEM("exclusiveData: Reserving data capacity...");
				// if requested capacity < existing size then reserve capacity for existing size instead
				data->reserve((capacity > _data->size()) ? capacity : _data->size());
MEM("exclusiveData: Reserved data capacity");
			}
			else {
				data->reserve(_data->size());
			}
MEM("exclusiveData: Copying existing data...");
			data->insert(data->begin(), _data->begin(), _data->end());
MEM("exclusiveData: Copied existing data");
MEM("exclusiveData: Assigning data to shared data pointer...");
			_data = SharedData(data);
MEM("exclusiveData: Assigned data to shared data pointer");
			_exclusive = true;
		}
		else {
			//MEM("Bytes is creating its own data because shared is empty");
			//data = new Data();
			//if (data == nullptr) {
			//	ERROR("Bytes failed to allocate empty data buffer");
			//	throw std::runtime_error("Failed to allocate empty data buffer");
			//}
			//_data = SharedData(data);
			//_exclusive = true;
MEM("exclusiveData: Creating new empty data...");
			newData(capacity);
MEM("exclusiveData: Created new empty data");
		}
	}
	else if (capacity > 0 && capacity > size()) {
		reserve(capacity);
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
	size_t hex_size = strlen(hex);
	exclusiveData(false, hex_size / 2);
	// need to clear data since we're appending below
	_data->clear();
	for (size_t i = 0; i < hex_size; i += 2) {
		uint8_t byte = (hex[i] % 32 + 9) % 25 * 16 + (hex[i+1] % 32 + 9) % 25;
		_data->push_back(byte);
	}
}

void Bytes::appendHex(const char* hex) {
	// if append is empty then do nothing
	if (hex == nullptr || hex[0] == 0) {
		return;
	}
	size_t hex_size = strlen(hex);
	exclusiveData(true, size() + (hex_size / 2));
	for (size_t i = 0; i < hex_size; i += 2) {
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
