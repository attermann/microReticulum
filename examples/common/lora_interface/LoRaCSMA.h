#pragma once

#ifdef ARDUINO
#include <RadioLib.h>

#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// LoRaCSMA — Software CSMA/CA with Clear Channel Assessment for LoRa
//
// Implements the algorithm described in docs/CSMA_CCA_Implementation.md:
//   - Digital carrier detection (DCD) via register polling / IRQ flags
//   - RSSI-based noise floor estimation and interference detection
//   - 802.11 DCF-style DIFS + contention window backoff (non-blocking)
//   - Airtime accounting with configurable lock thresholds
//   - Channel utilization tracking
//
// Usage:
//   1. Call init() once after radio is configured and online.
//   2. Call update(millis()) every loop iteration.
//      Returns true when the channel is clear and backoff is complete.
//   3. After a successful transmit, call notify_tx_complete(bytes, millis()).
// ---------------------------------------------------------------------------

class LoRaCSMA {

public:
    // Radio chip family — determines which DCD polling method to use
    enum class ChipFamily { SX127x, SX126x };

    // Radio parameters used to derive all CSMA timing constants.
    // symbol_time_ms is computed by init() and should be left zero on input.
    struct RadioParams {
        float bw_khz          = 125.0f;
        int   sf              = 8;
        int   cr              = 5;
        int   preamble_symbols = 20;
        float bitrate_bps     = 0.0f;
        // Computed by init() — do not set manually
        float symbol_time_ms  = 0.0f;
    };

    // Must be called once after the radio is initialised and online.
    // radio_127x: non-null only for SX127x boards (needed for getModemStatus()).
    // radio:      PhysicalLayer pointer used for all other radio calls.
    void init(ChipFamily family,
              SX127x*       radio_127x,
              PhysicalLayer* radio,
              const RadioParams& params);

    // ---------------------------------------------------------------------------
    // Main interface — called by LoRaInterface every loop() iteration.
    // Polls DCD/RSSI (throttled to every 3 ms) and steps the CSMA state machine.
    // Returns true exactly once per transmission clearance: caller must transmit
    // now, then call notify_tx_complete().
    // ---------------------------------------------------------------------------
    bool update(uint32_t now_ms);

    // Call immediately after a successful transmit to account airtime and
    // reset the CSMA state machine for the next packet.
    void notify_tx_complete(size_t bytes, uint32_t now_ms);

    // ---------------------------------------------------------------------------
    // Status accessors
    // ---------------------------------------------------------------------------

    // True if LoRaInterface should call startReceive() this iteration
    // (set after a false preamble is cleared on SX126x).
    bool needs_rearm() const { return _needs_rearm; }

    // True while own airtime exceeds the configured threshold — TX is halted.
    bool airtime_locked() const { return _airtime_locked; }

    // Current estimated noise floor in dBm (-292 = not yet sampled).
    float noise_floor() const { return _noise_floor; }

    // Fraction of time the channel was observed busy (DCD true) since init.
    float local_channel_util() const;

    // local_channel_util + own_airtime fraction, capped at 1.0.
    float total_channel_util() const;

private:
    // --- DCD ---
    bool poll_dcd(uint32_t now_ms);
    bool poll_dcd_127x();
    bool poll_dcd_126x(uint32_t now_ms);
    bool medium_free() const;

    // --- RSSI / noise floor ---
    void sample_rssi(uint32_t now_ms);

    // --- Timing ---
    void compute_timing();

    // --- Contention window ---
    int  select_cw_band() const;

    // --- Airtime ---
    float compute_toa_ms(size_t bytes) const;
    void  tick_airtime_bins(uint32_t now_ms);

    // -----------------------------------------------------------------------
    // Hardware references
    // -----------------------------------------------------------------------
    ChipFamily      _family     = ChipFamily::SX126x;
    SX127x*         _radio_127x = nullptr;   // non-null iff SX127x board
    PhysicalLayer*  _radio      = nullptr;
    RadioParams     _params     = {};

    // -----------------------------------------------------------------------
    // Timing constants (computed once in compute_timing())
    // -----------------------------------------------------------------------
    float  _slot_ms          = 0.0f;
    float  _difs_ms          = 0.0f;
    float  _preamble_time_ms = 0.0f;
    float  _header_time_ms   = 0.0f;

    // -----------------------------------------------------------------------
    // DCD state
    // -----------------------------------------------------------------------
    bool     _dcd              = false;
    // SX126x false-preamble tracking
    bool     _preamble_pending = false;
    uint32_t _preamble_at_ms   = 0;
    bool     _needs_rearm      = false;

    // -----------------------------------------------------------------------
    // RSSI / noise floor
    // -----------------------------------------------------------------------
    static constexpr int     NOISE_SAMPLES  = 128;
    static constexpr float   NOISE_MARGIN   = 11.0f;   // dB above noise floor
    static constexpr float   INTF_RSSI_MAX  = -83.0f;  // dBm recal threshold
    static constexpr uint32_t INTF_RECAL_MS = 2500;    // recalibration holdoff

    float    _current_rssi  = 0.0f;
    float    _noise_floor   = -292.0f;   // sentinel: not yet sampled
    float    _rssi_sum      = 0.0f;
    int      _rssi_count    = 0;
    bool     _interference  = false;
    uint32_t _intf_start_ms = 0;

    // -----------------------------------------------------------------------
    // CSMA state machine
    // Timestamps stored as int64_t so -1 serves as an unambiguous "unset"
    // sentinel; arithmetic is performed with (uint32_t) casts for rollover safety.
    // -----------------------------------------------------------------------
    int32_t  _csma_cw         = -1;      // slots; -1 = not yet drawn
    float    _cw_wait_target  = 0.0f;    // total CW wait required (ms)
    int64_t  _difs_wait_start = -1;      // millis() when DIFS started; -1 = not started
    int64_t  _cw_wait_start   = -1;      // millis() when current sub-interval started
    float    _cw_wait_passed  = 0.0f;    // accumulated free-medium time (ms)

    // -----------------------------------------------------------------------
    // Airtime accounting
    // -----------------------------------------------------------------------
    static constexpr int      AIRTIME_BINS    = 20;
    static constexpr uint32_t AIRTIME_BIN_MS  = 1000;  // 1-second bins
    // Short-term lock: if own airtime > 50 % of the last 2 seconds
    static constexpr float    AIRTIME_ST_LOCK = 0.50f;
    // Long-term lock: if own airtime > 25 % of the last 20 seconds
    static constexpr float    AIRTIME_LT_LOCK = 0.25f;

    float    _airtime_bins[AIRTIME_BINS] = {};  // ms of TX per bin
    int      _current_bin    = 0;
    uint32_t _bin_start_ms   = 0;
    float    _short_term_frac = 0.0f;   // 0..1
    float    _long_term_frac  = 0.0f;   // 0..1
    bool     _airtime_locked  = false;

    // -----------------------------------------------------------------------
    // Channel utilization tracking
    // -----------------------------------------------------------------------
    uint32_t _dcd_true_count  = 0;
    uint32_t _dcd_total_count = 0;

    // -----------------------------------------------------------------------
    // Polling throttle
    // -----------------------------------------------------------------------
    uint32_t _last_poll_ms = 0;
    static constexpr uint32_t POLL_INTERVAL_MS = 3;
};

#endif // ARDUINO
