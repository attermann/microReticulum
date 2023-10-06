#include "Bytes.h"

using namespace RNS;

void Bytes::ownData() {
	if (!_data) {
		newData();
	}
	else if (!_owner) {
		Data *data;
		if (!_data->empty()) {
			//extreme("Bytes is creating a writable copy of its shared data");
			data = new Data(*_data.get());
		}
		else {
			//extreme("Bytes is creating its own data because shared is empty");
			data = new Data();
		}
		_data = SharedData(data);
		_owner = true;
	}
}

int8_t Bytes::compare(const Bytes &bytes) const {
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
	else {
		return 1;
	}
}

char const hex_upper_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
char const hex_lower_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

std::string Bytes::toHex(bool upper /*= true*/) const {
	if (!_data) {
		return "";
	}
	std::string hex;
	for (uint8_t byte : *_data) {
		if (upper) {
			hex += hex_upper_chars[ (byte & 0xF0) >> 4];
			hex += hex_upper_chars[ (byte & 0x0F) >> 0];
		}
		else {
			hex += hex_lower_chars[ (byte & 0xF0) >> 4];
			hex += hex_lower_chars[ (byte & 0x0F) >> 0];
		}
	}
	return hex;
}

void Bytes::assignHex(const char* hex) {
	// if assignment is empty then clear data and don't bother creating new
	if (hex == nullptr || hex[0] == 0) {
		_data = nullptr;
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
	ownData();
	size_t len = strlen(hex);
	for (size_t i = 0; i < len; i += 2) {
		uint8_t byte = (hex[i] % 32 + 9) % 25 * 16 + (hex[i+1] % 32 + 9) % 25;
		_data->push_back(byte);
	}
}
