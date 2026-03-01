#pragma once

#include "../Bytes.h"
//#include "../Log.h"

#include <stdexcept>

namespace RNS { namespace Cryptography {

	class PKCS7 {

	public:

		static const size_t BLOCKSIZE = 16;

		static inline const Bytes pad(const Bytes& data, size_t bs = BLOCKSIZE) {
			Bytes padded(data);
			inplace_pad(padded, bs);
			return padded;
		}

		static inline const Bytes unpad(const Bytes& data, size_t bs = BLOCKSIZE) {
			Bytes unpadded(data);
			inplace_unpad(unpadded, bs);
			return unpadded;
		}

		// updates passed buffer
		static inline void inplace_pad(Bytes& data, size_t bs = BLOCKSIZE) {
			size_t len = data.size();
			//DEBUGF("PKCS7::pad: len: %zu", len);
			size_t padlen = bs - (len % bs);
			//DEBUGF("PKCS7::pad: pad len: %zu", padlen);
			// create zero-filled byte padding array of size padlen
			//p v = bytes([padlen])
			//uint8_t pad[padlen] = {0};
			uint8_t pad[padlen];
			memset(pad, 0, padlen);
			// set last byte of padding array to size of padding
			pad[padlen-1] = (uint8_t)padlen;
			// concatenate data with padding
			//p return data+v*padlen
			data.append(pad, padlen);
			//DEBUGF("PKCS7::pad: data size: %zu", data.size());
		}

		// updates passed buffer
		static inline void inplace_unpad(Bytes& data, size_t bs = BLOCKSIZE) {
			size_t len = data.size();
			//DEBUGF("PKCS7::unpad: len: %zu", len);
			// read last byte which is pad length
			//pad = data[-1]
			size_t padlen = (size_t)data.data()[data.size()-1];
			//DEBUGF("PKCS7::unpad: pad len: %zu", padlen);
			if (padlen > bs) {
				throw std::runtime_error("Cannot unpad, invalid padding length of " + std::to_string(padlen) + " bytes");
			}
			// truncate data to strip padding
			//return data[:len-padlen]
			data.resize(len - padlen);
			//DEBUGF("PKCS7::unpad: data size: %zu", data.size());
		}

	};

} }
