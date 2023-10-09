#pragma once

#include "../Bytes.h"
//#include "../Log.h"

#include <stdexcept>

namespace RNS { namespace Cryptography {

	class PKCS7 {

	public:

		static const size_t BLOCKSIZE = 16;

		static inline Bytes pad(const Bytes &data, size_t bs = BLOCKSIZE) {
			Bytes padded(data);
			inplace_pad(padded, bs);
			return padded;
		}

		static inline Bytes unpad(const Bytes &data, size_t bs = BLOCKSIZE) {
			Bytes unpadded(data);
			inplace_unpad(unpadded, bs);
			return unpadded;
		}

		// updates passed buffer
		static inline void inplace_pad(Bytes &data, size_t bs = BLOCKSIZE) {
			size_t len = data.size();
			//debug("PKCS7::pad: len: " + std::to_string(len));
			size_t padlen = bs - (len % bs);
			//debug("PKCS7::pad: pad len: " + std::to_string(padlen));
			// create byte array of size n?
			//v = bytes([padlen])
			uint8_t pad[padlen] = {0};
			pad[padlen-1] = (uint8_t)padlen;
			//return data+v*padlen
			data.append(pad, padlen);
			//debug("PKCS7::pad: data size: " + std::to_string(data.size()));
		}

		// updates passed buffer
		static inline void inplace_unpad(Bytes &data, size_t bs = BLOCKSIZE) {
			size_t len = data.size();
			//debug("PKCS7::unpad: len: " + std::to_string(len));
			// last byte is pad length
			//pad = data[-1]
			size_t padlen = (size_t)data.data()[data.size()-1];
			//debug("PKCS7::unpad: pad len: " + std::to_string(padlen));
			if (padlen > bs) {
				throw std::runtime_error("Cannot unpad, invalid padding length of " + std::to_string(padlen) + " bytes");
			}
			//return data[:len-padlen]
			data.resize(len - padlen);
			//debug("PKCS7::unpad: data size: " + std::to_string(data.size()));
		}

	};

} }
