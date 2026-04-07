#include "LoRaCSMA.h"

#ifdef ARDUINO

#include "../src/Log.h"
#include "../src/Cryptography/Random.h"

#include <math.h>

using namespace RNS;

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void LoRaCSMA::init(ChipFamily family,
                    SX127x*       radio_127x,
                    PhysicalLayer* radio,
                    const RadioParams& params) {
    _family     = family;
    _radio_127x = radio_127x;
    _radio      = radio;
    _params     = params;
    compute_timing();
    _bin_start_ms = (uint32_t)millis();
    TRACEF("LoRaCSMA: slot=%.1fms difs=%.1fms preamble=%.1fms header=%.1fms",
           _slot_ms, _difs_ms, _preamble_time_ms, _header_time_ms);
}

// ---------------------------------------------------------------------------
// compute_timing — derives all CSMA time constants from radio parameters
// ---------------------------------------------------------------------------

void LoRaCSMA::compute_timing() {
    float bw_hz  = _params.bw_khz * 1000.0f;
    float sym_ms = 1000.0f / (bw_hz / (float)(1 << _params.sf));
    float slot   = sym_ms * 12.0f;
    // Minimum slot: 6 ms for fast bitrates (>1200 bps), 24 ms otherwise
    float min_slot = (_params.bitrate_bps > 1200.0f) ? 6.0f : 24.0f;
    _slot_ms          = fmaxf(min_slot, fminf(slot, 100.0f));
    _difs_ms          = 2.0f * _slot_ms;
    _preamble_time_ms = ceilf(_params.preamble_symbols * sym_ms);
    _header_time_ms   = ceilf(20.0f * sym_ms);
    _params.symbol_time_ms = sym_ms;
}

// ---------------------------------------------------------------------------
// DCD polling
// ---------------------------------------------------------------------------

bool LoRaCSMA::poll_dcd_127x() {
    // REG_MODEM_STAT (0x18): bit 0 = signal detected, bit 1 = signal synced
    return (_radio_127x->getModemStatus() & 0x03) != 0;
}

bool LoRaCSMA::poll_dcd_126x(uint32_t now_ms) {
    // getIrqFlags() returns abstracted flags via SX126x's irqMap —
    // RADIOLIB_IRQ_PREAMBLE_DETECTED and RADIOLIB_IRQ_HEADER_VALID work
    // through PhysicalLayer* without a cast.
    uint32_t irq       = _radio->getIrqFlags();
    bool header_valid  = (irq & (1UL << RADIOLIB_IRQ_HEADER_VALID))      != 0;
    bool preamble_det  = (irq & (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED)) != 0;

    if (header_valid) {
        // High-confidence carrier — valid LoRa header received
        _preamble_pending = false;
        return true;
    }

    if (preamble_det) {
        if (!_preamble_pending) {
            // First detection: start false-preamble timeout
            _preamble_pending = true;
            _preamble_at_ms   = now_ms;
            return true;
        }
        // Preamble still latched — check for false-preamble timeout
        uint32_t elapsed = now_ms - _preamble_at_ms;
        uint32_t timeout = (uint32_t)(_preamble_time_ms + _header_time_ms) + 2;
        if (elapsed > timeout) {
            // False preamble: clear the hardware latch and signal LoRaInterface
            // to call startReceive() to escape the latched modem state
            _preamble_pending = false;
            _radio->clearIrqFlags(1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED);
            _needs_rearm = true;
            TRACE("LoRaCSMA: false preamble cleared, requesting RX re-arm");
            return false;
        }
        return true;
    }

    _preamble_pending = false;
    return false;
}

bool LoRaCSMA::poll_dcd(uint32_t now_ms) {
    if (_family == ChipFamily::SX127x) {
        return poll_dcd_127x();
    } else {
        return poll_dcd_126x(now_ms);
    }
}

bool LoRaCSMA::medium_free() const {
    return !_dcd && !(_interference);
}

// ---------------------------------------------------------------------------
// RSSI / noise floor
// ---------------------------------------------------------------------------

void LoRaCSMA::sample_rssi(uint32_t now_ms) {
    // Only sample when channel is idle
    if (_dcd) return;

    if (_current_rssi >= _noise_floor + NOISE_MARGIN) {
        // Elevated RSSI — possible wideband interference
        _interference = true;
        if (_intf_start_ms == 0) {
            _intf_start_ms = now_ms;
        }
        // If persistently mild (below INTF_RSSI_MAX) for INTF_RECAL_MS,
        // reset the baseline — we've likely moved to a noisier environment
        if ((now_ms - _intf_start_ms) >= INTF_RECAL_MS
                && _current_rssi < INTF_RSSI_MAX) {
            TRACE("LoRaCSMA: recalibrating noise floor");
            _noise_floor = -292.0f;
            _rssi_sum    = 0.0f;
            _rssi_count  = 0;
        }
        return;
    }

    // Clear interference flag when RSSI drops back below threshold
    _interference  = false;
    _intf_start_ms = 0;

    // Accumulate running sum; update noise floor every NOISE_SAMPLES readings
    _rssi_sum += _current_rssi;
    _rssi_count++;
    if (_rssi_count >= NOISE_SAMPLES) {
        _noise_floor = _rssi_sum / (float)NOISE_SAMPLES;
        _rssi_sum    = 0.0f;
        _rssi_count  = 0;
        TRACEF("LoRaCSMA: noise floor updated to %.1f dBm", _noise_floor);
    }
}

// ---------------------------------------------------------------------------
// Contention window band selection
// ---------------------------------------------------------------------------

int LoRaCSMA::select_cw_band() const {
    float pct = _short_term_frac * 100.0f;
    if      (pct <  7.0f) return 0;   // band 1: CW in [0, 14]
    else if (pct < 31.0f) return 1;   // band 2: CW in [15, 29]
    else if (pct < 54.0f) return 2;   // band 3: CW in [30, 44]
    else                   return 3;   // band 4: CW in [45, 59]
}

// ---------------------------------------------------------------------------
// Airtime accounting
// ---------------------------------------------------------------------------

float LoRaCSMA::compute_toa_ms(size_t bytes) const {
    int   SF  = _params.sf;
    int   CR  = _params.cr;
    int   P   = _params.preamble_symbols;
    float sms = _params.symbol_time_ms;
    bool  ldr = (sms >= 16.0f);   // low datarate optimisation

    float payload_sym;
    if (_family == ChipFamily::SX126x && SF < 7) {
        // SX126x, SF < 7: slightly different preamble overhead
        int num    = 8 * (int)bytes + 16 - 4 * SF + 20;
        int den    = 4 * SF;
        payload_sym = ceilf((float)num / (float)den) * (float)CR;
        return (payload_sym + (float)P + 2.25f + 8.0f) * sms;
    } else {
        int lv  = ldr ? 1 : 0;
        int num = 8 * (int)bytes + 16 - 4 * SF + 8 + 20;
        int den = 4 * (SF - 2 * lv);
        payload_sym = ceilf((float)num / (float)den) * (float)CR;
        return (payload_sym + (float)P + 0.25f + 8.0f) * sms;
    }
}

void LoRaCSMA::tick_airtime_bins(uint32_t now_ms) {
    if ((now_ms - _bin_start_ms) >= AIRTIME_BIN_MS) {
        _bin_start_ms = now_ms;
        _current_bin  = (_current_bin + 1) % AIRTIME_BINS;
        _airtime_bins[_current_bin] = 0.0f;   // clear the new bin
    }

    // Short-term: average of current + previous bin (covers last ~2 seconds)
    int prev = (_current_bin - 1 + AIRTIME_BINS) % AIRTIME_BINS;
    _short_term_frac = (_airtime_bins[_current_bin] + _airtime_bins[prev])
                       / (2.0f * (float)AIRTIME_BIN_MS);

    // Long-term: sum of all bins over AIRTIME_BINS seconds
    float sum = 0.0f;
    for (int i = 0; i < AIRTIME_BINS; i++) sum += _airtime_bins[i];
    _long_term_frac = sum / ((float)AIRTIME_BINS * (float)AIRTIME_BIN_MS);

    _airtime_locked = (_short_term_frac > AIRTIME_ST_LOCK)
                   || (_long_term_frac  > AIRTIME_LT_LOCK);
}

// ---------------------------------------------------------------------------
// update — called every loop(); returns true when clear-to-send
// ---------------------------------------------------------------------------

bool LoRaCSMA::update(uint32_t now_ms) {
    _needs_rearm = false;

    // --- Hardware polling (throttled to POLL_INTERVAL_MS) ---
    if ((now_ms - _last_poll_ms) >= POLL_INTERVAL_MS) {
        _last_poll_ms = now_ms;

        _dcd          = poll_dcd(now_ms);
        _current_rssi = _radio->getRSSI();
        sample_rssi(now_ms);

        _dcd_total_count++;
        if (_dcd) _dcd_true_count++;

        tick_airtime_bins(now_ms);
    }

    if (_airtime_locked) return false;

    // --- CSMA state machine ---

    // Step 2: draw a random backoff on the first call for this packet
    if (_csma_cw == -1) {
        int band       = select_cw_band();
        int cw_min     = band * 15;
        _csma_cw       = cw_min + (int)(Cryptography::randomnum(15));
        _cw_wait_target = (float)_csma_cw * _slot_ms;
        TRACEF("LoRaCSMA: band=%d cw=%d target=%.1fms", band, _csma_cw, _cw_wait_target);
    }

    bool free = medium_free();

    // Step 4: medium is busy — freeze CW accumulation and restart DIFS
    if (!free) {
        if (_cw_wait_start != -1) {
            // Preserve elapsed CW time (frozen backoff)
            _cw_wait_passed += (float)(now_ms - (uint32_t)_cw_wait_start);
        }
        _difs_wait_start = -1;
        _cw_wait_start   = -1;
        return false;
    }

    // Step 3: start DIFS countdown
    if (_difs_wait_start == -1) {
        _difs_wait_start = (int64_t)now_ms;
        return false;
    }

    // Step 5: wait out DIFS
    if ((float)(now_ms - (uint32_t)_difs_wait_start) < _difs_ms) {
        return false;
    }

    // Step 6: CW countdown — accumulate free-medium time
    if (_cw_wait_start == -1) {
        _cw_wait_start = (int64_t)now_ms;
        return false;
    }
    _cw_wait_passed += (float)(now_ms - (uint32_t)_cw_wait_start);
    _cw_wait_start   = (int64_t)now_ms;

    if (_cw_wait_passed < _cw_wait_target) {
        return false;
    }

    // Step 7: CW complete — signal caller to transmit now
    return true;
}

// ---------------------------------------------------------------------------
// notify_tx_complete — call after every successful transmit
// ---------------------------------------------------------------------------

void LoRaCSMA::notify_tx_complete(size_t bytes, uint32_t now_ms) {
    float toa = compute_toa_ms(bytes);
    _airtime_bins[_current_bin] += toa;

    TRACEF("LoRaCSMA: TX done, toa=%.1fms airtime_st=%.1f%%",
           toa, _short_term_frac * 100.0f);

    // Reset CSMA state for the next packet
    _cw_wait_passed  = 0.0f;
    _csma_cw         = -1;
    _difs_wait_start = -1;
    _cw_wait_start   = -1;
    (void)now_ms;
}

// ---------------------------------------------------------------------------
// Status accessors
// ---------------------------------------------------------------------------

float LoRaCSMA::local_channel_util() const {
    if (_dcd_total_count == 0) return 0.0f;
    return (float)_dcd_true_count / (float)_dcd_total_count;
}

float LoRaCSMA::total_channel_util() const {
    float total = local_channel_util() + _long_term_frac;
    return (total > 1.0f) ? 1.0f : total;
}

#endif // ARDUINO
