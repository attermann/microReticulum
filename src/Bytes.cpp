#include "Bytes.h"

using namespace RNS;

// Creates new shared data for instance
// - If capacity is specified (>0) then create empty shared data with initial reserved capacity
// - If capacity is not specified (<=0) then create empty shared data with no initial capacity
void Bytes::newData(size_t capacity /*= 0*/) {
//MEMF("Bytes is creating own data with capacity %u", capacity);
//MEM("newData: Creating new data...");
	Data* data = new Data();
	if (data == nullptr) {
		ERROR("Bytes failed to allocate empty data buffer");
		throw std::runtime_error("Failed to allocate empty data buffer");
	}
//MEM("newData: Created new data");
	if (capacity > 0) {
//MEMF("newData: Reserving data capacity of %u...", capacity);
		data->reserve(capacity);
//MEM("newData: Reserved data capacity");
	}
//MEM("newData: Assigning data to shared data pointer...");
	_data = SharedData(data);
//MEM("newData: Assigned data to shared data pointer");
	_exclusive = true;
}

// Ensures that instance has exclusive shared data
// - If instance has no shared data then create new shared data
// - If instance does not have exclusive on shared data that is not empty then make a copy of shared data (if requests) and reserve capacity (if requested)
// - If instance does not have exclusive on shared data that is empty then create new shared data
// - If instance already has exclusive on shared data then do nothing except reserve capacity (if requested)
void Bytes::exclusiveData(bool copy /*= true*/, size_t capacity /*= 0*/) {
	if (!_data) {
		newData(capacity);
	}
	else if (!_exclusive) {
		if (copy && !_data->empty()) {
			//TRACE("Bytes is creating a writable copy of its shared data");
			//Data* data = new Data(*_data.get());
//MEM("exclusiveData: Creating new data...");
			Data* data = new Data();
			if (data == nullptr) {
				ERROR("Bytes failed to duplicate data buffer");
				throw std::runtime_error("Failed to duplicate data buffer");
			}
//MEM("exclusiveData: Created new data");
			if (capacity > 0) {
//MEMF("exclusiveData: Reserving data capacity of %u...", capacity);
				// if requested capacity < existing size then reserve capacity for existing size instead
				data->reserve((capacity > _data->size()) ? capacity : _data->size());
//MEM("exclusiveData: Reserved data capacity");
			}
			else {
				data->reserve(_data->size());
			}
//MEM("exclusiveData: Copying existing data...");
			data->insert(data->begin(), _data->begin(), _data->end());
//MEM("exclusiveData: Copied existing data");
//MEM("exclusiveData: Assigning data to shared data pointer...");
			_data = SharedData(data);
//MEM("exclusiveData: Assigned data to shared data pointer");
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
//MEM("exclusiveData: Creating new empty data...");
			newData(capacity);
//MEM("exclusiveData: Created new empty data");
		}
	}
	else if (capacity > 0 && capacity > size()) {
		reserve(capacity);
	}
}

int Bytes::compare(const Bytes& bytes) const {
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

int Bytes::compare(const uint8_t* buf, size_t size) const {
	if (!_data && size == 0) {
		return 0;
	}
	else if (!_data) {
		return -1;
	}
	int cmp = memcmp(_data->data(), buf, (_data->size() < size) ? _data->size() : size);
	if (cmp == 0 && _data->size() < size) {
		return -1;
	}
	else if (cmp == 0 && _data->size() > size) {
		return 1;
	}
	return cmp;
}

void Bytes::assignHex(const uint8_t* hex, size_t hex_size) {
	// if assignment is empty then clear data and don't bother creating new
	if (hex == nullptr || hex_size <= 0) {
		_data = nullptr;
		_exclusive = true;
		return;
	}
	// Truncate to even length (hex bytes come in pairs)
	hex_size &= ~(size_t)1;
	if (hex_size == 0) {
		_data = nullptr;
		_exclusive = true;
		return;
	}
	exclusiveData(false, hex_size / 2);
	// need to clear data since we're appending below
	_data->clear();
	for (size_t i = 0; i < hex_size; i += 2) {
		uint8_t byte = (hex[i] % 32 + 9) % 25 * 16 + (hex[i+1] % 32 + 9) % 25;
		_data->push_back(byte);
	}
}

void Bytes::appendHex(const uint8_t* hex, size_t hex_size) {
	// if append is empty then do nothing
	if (hex == nullptr || hex_size <= 0) {
		return;
	}
	// Truncate to even length (hex bytes come in pairs)
	hex_size &= ~(size_t)1;
	if (hex_size == 0) {
		return;
	}
	exclusiveData(true, size() + (hex_size / 2));
	for (size_t i = 0; i < hex_size; i += 2) {
		uint8_t byte = (hex[i] % 32 + 9) % 25 * 16 + (hex[i+1] % 32 + 9) % 25;
		_data->push_back(byte);
	}
}

const char hex_upper_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
const char hex_lower_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static const char base64_chars[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int8_t b64_decode_char(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;  // invalid / padding
}

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

std::string Bytes::toBase64() const {
    if (!_data) {
        return "";
    }
    const uint8_t* in = _data->data();
    size_t in_size = _data->size();
    std::string out;
    out.reserve(((in_size + 2) / 3) * 4);
    for (size_t i = 0; i < in_size; i += 3) {
        uint8_t b0 = in[i];
        uint8_t b1 = (i + 1 < in_size) ? in[i + 1] : 0;
        uint8_t b2 = (i + 2 < in_size) ? in[i + 2] : 0;
        out += base64_chars[(b0 >> 2) & 0x3F];
        out += base64_chars[((b0 << 4) | (b1 >> 4)) & 0x3F];
        out += (i + 1 < in_size) ? base64_chars[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=';
        out += (i + 2 < in_size) ? base64_chars[b2 & 0x3F] : '=';
    }
    return out;
}

void Bytes::assignBase64(const uint8_t* b64, size_t b64_size) {
    if (b64 == nullptr || b64_size == 0) {
        _data = nullptr;
        _exclusive = true;
        return;
    }
    // strip trailing padding
    while (b64_size > 0 && b64[b64_size - 1] == '=') {
        --b64_size;
    }
    size_t out_size = (b64_size * 3) / 4;
    exclusiveData(false, out_size);
    _data->clear();
    for (size_t i = 0; i < b64_size; ) {
        int8_t v0 = (i < b64_size) ? b64_decode_char(b64[i++]) : 0;
        int8_t v1 = (i < b64_size) ? b64_decode_char(b64[i++]) : 0;
        int8_t v2 = (i < b64_size) ? b64_decode_char(b64[i++]) : -1;
        int8_t v3 = (i < b64_size) ? b64_decode_char(b64[i++]) : -1;
        if (v0 < 0 || v1 < 0) break;
        _data->push_back((uint8_t)((v0 << 2) | (v1 >> 4)));
        if (v2 >= 0) _data->push_back((uint8_t)((v1 << 4) | (v2 >> 2)));
        if (v3 >= 0) _data->push_back((uint8_t)((v2 << 6) | v3));
    }
    if (_data->empty()) {
        _data = nullptr;
        _exclusive = true;
    }
}

void Bytes::appendBase64(const uint8_t* b64, size_t b64_size) {
    if (b64 == nullptr || b64_size == 0) {
        return;
    }
    // strip trailing padding
    while (b64_size > 0 && b64[b64_size - 1] == '=') {
        --b64_size;
    }
    size_t out_size = (b64_size * 3) / 4;
    exclusiveData(true, size() + out_size);
    for (size_t i = 0; i < b64_size; ) {
        int8_t v0 = (i < b64_size) ? b64_decode_char(b64[i++]) : 0;
        int8_t v1 = (i < b64_size) ? b64_decode_char(b64[i++]) : 0;
        int8_t v2 = (i < b64_size) ? b64_decode_char(b64[i++]) : -1;
        int8_t v3 = (i < b64_size) ? b64_decode_char(b64[i++]) : -1;
        if (v0 < 0 || v1 < 0) break;
        _data->push_back((uint8_t)((v0 << 2) | (v1 >> 4)));
        if (v2 >= 0) _data->push_back((uint8_t)((v1 << 4) | (v2 >> 2)));
        if (v3 >= 0) _data->push_back((uint8_t)((v2 << 6) | v3));
    }
}

// mid
Bytes Bytes::mid(size_t beginpos, size_t len) const {
	if (!_data || beginpos >= size()) {
		return NONE;
	}
	size_t remaining = size() - beginpos;
	if (len > remaining) {
		len = remaining;
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
