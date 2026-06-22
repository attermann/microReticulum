/*
 * Copyright (c) 2026 Chad Attermann
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

#include "Compress.h"

extern "C" {
#include "heatshrink/heatshrink_encoder.h"
#include "heatshrink/heatshrink_decoder.h"
}

namespace RNS { namespace Utilities {

	// Output scratch — sized to amortize encoder_poll / decoder_poll calls.
	// Each loop iteration drains up to this many bytes from heatshrink before
	// returning to feeding more input, keeping the call count bounded.
	static constexpr size_t POLL_CHUNK = 128;

	Bytes Compress::encode(const uint8_t* in_data, size_t in_len) {
		if (!in_data || in_len == 0) return Bytes();

		// Encoder struct is ~3 KB with WINDOW_BITS=9; stack-local is fine
		// on ESP32/nRF52 task stacks (8 KB+). If this ever migrates to a
		// tighter target, switch to `new heatshrink_encoder` + delete.
		heatshrink_encoder hse;
		heatshrink_encoder_reset(&hse);

		// Pre-reserve worst-case-input bytes. If we ever produce more than
		// in_len, compression is unprofitable — bail out so callers send the
		// uncompressed payload instead of wrapping a larger blob.
		Bytes out(in_len);
		uint8_t scratch[POLL_CHUNK];

		// Feed phase: sink all input into the encoder, draining output as
		// the encoder's internal buffer fills.
		size_t in_pos = 0;
		while (in_pos < in_len) {
			size_t sunk = 0;
			HSE_sink_res sres = heatshrink_encoder_sink(
				&hse,
				const_cast<uint8_t*>(in_data + in_pos),
				in_len - in_pos,
				&sunk);
			if (sres < 0) return Bytes();
			in_pos += sunk;

			HSE_poll_res pres;
			do {
				size_t produced = 0;
				pres = heatshrink_encoder_poll(&hse, scratch, sizeof(scratch), &produced);
				if (pres < 0) return Bytes();
				if (produced > 0) {
					if (out.size() + produced >= in_len) return Bytes();
					out.append(scratch, produced);
				}
			} while (pres == HSER_POLL_MORE);
		}

		// Finish phase: tell the encoder the input is done and drain the
		// remaining bits. May still produce output across several polls.
		HSE_finish_res fres = heatshrink_encoder_finish(&hse);
		while (fres == HSER_FINISH_MORE) {
			size_t produced = 0;
			HSE_poll_res pres = heatshrink_encoder_poll(&hse, scratch, sizeof(scratch), &produced);
			if (pres < 0) return Bytes();
			if (produced > 0) {
				if (out.size() + produced >= in_len) return Bytes();
				out.append(scratch, produced);
			}
			fres = heatshrink_encoder_finish(&hse);
		}
		if (fres != HSER_FINISH_DONE) return Bytes();

		return out;
	}

	Bytes Compress::decode(const uint8_t* in_data, size_t in_len, size_t max_out) {
		if (!in_data || in_len == 0 || max_out == 0) return Bytes();

		// Decoder static size = 2^WINDOW_BITS + small state ≈ 1 KB.
		heatshrink_decoder hsd;
		heatshrink_decoder_reset(&hsd);

		Bytes out;
		uint8_t scratch[POLL_CHUNK];

		size_t in_pos = 0;
		while (in_pos < in_len) {
			size_t sunk = 0;
			HSD_sink_res sres = heatshrink_decoder_sink(
				&hsd,
				const_cast<uint8_t*>(in_data + in_pos),
				in_len - in_pos,
				&sunk);
			if (sres < 0) return Bytes();
			in_pos += sunk;

			HSD_poll_res pres;
			do {
				size_t produced = 0;
				pres = heatshrink_decoder_poll(&hsd, scratch, sizeof(scratch), &produced);
				if (pres < 0) return Bytes();
				if (produced > 0) {
					if (out.size() + produced > max_out) return Bytes();
					out.append(scratch, produced);
				}
			} while (pres == HSDR_POLL_MORE);
		}

		HSD_finish_res fres = heatshrink_decoder_finish(&hsd);
		while (fres == HSDR_FINISH_MORE) {
			size_t produced = 0;
			HSD_poll_res pres = heatshrink_decoder_poll(&hsd, scratch, sizeof(scratch), &produced);
			if (pres < 0) return Bytes();
			if (produced > 0) {
				if (out.size() + produced > max_out) return Bytes();
				out.append(scratch, produced);
			}
			fres = heatshrink_decoder_finish(&hsd);
		}
		if (fres != HSDR_FINISH_DONE) return Bytes();

		return out;
	}

} }
