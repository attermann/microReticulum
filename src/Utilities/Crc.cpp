#include "Crc.h"

using namespace RNS::Utilities;

/*static*/ uint32_t Crc::crc32(uint32_t crc, const uint8_t* buf, size_t size) {
	const unsigned char *data = (const unsigned char *)buf;
	if (data == NULL)
		return 0;
	crc ^= 0xffffffff;
	while (size--) {
		crc ^= *data++;
		for (unsigned k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
	}
	return crc ^ 0xffffffff;
}
