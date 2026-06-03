/*
 * Copyright (c) 2023 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

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
