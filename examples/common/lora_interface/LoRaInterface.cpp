#include "LoRaInterface.h"

#include "../src/Log.h"
#include "../src/Utilities/OS.h"

#include <memory>

// ---------------------------------------------------------------------------
// Board-specific pin definitions
// ---------------------------------------------------------------------------

#if defined(BOARD_TBEAM) || defined(BOARD_LORA32_V21)
// LILYGO T-Beam V1.X / LoRa32 V2.1 — SX1276
// RadioLib Module(cs, irq=DIO0, rst, gpio=DIO1)
#define RADIO_SCLK_PIN               5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN              26   // IRQ (RxDone/TxDone)
#define RADIO_RST_PIN               23
#define RADIO_DIO1_PIN              33   // gpio (optional, passed to Module)

#elif defined(BOARD_RAK4631)
// RAK4631 (WisCore RAK4630) — SX1262
// RadioLib Module(cs, irq=DIO1, rst, busy)
#define RADIO_SCLK_PIN              43
#define RADIO_MISO_PIN              45
#define RADIO_MOSI_PIN              44
#define RADIO_CS_PIN                42
#define RADIO_DIO1_PIN              47   // IRQ (all SX1262 IRQs route to DIO1)
#define RADIO_RST_PIN               38
#define RADIO_BUSY_PIN              46

#elif defined(BOARD_HELTEC_V3)
// Heltec WiFi LoRa 32 V3 — ESP32-S3 + SX1262
// RadioLib Module(cs, irq=DIO1, rst, busy)
// Note: Heltec BSP names this pin "DIO0" but it is physically DIO1 on the SX1262
#define RADIO_SCLK_PIN               9
#define RADIO_MISO_PIN              11
#define RADIO_MOSI_PIN              10
#define RADIO_CS_PIN                 8
#define RADIO_DIO1_PIN              14   // IRQ
#define RADIO_RST_PIN               12
#define RADIO_BUSY_PIN              13

#elif defined(BOARD_HELTEC_V4)
// Heltec WiFi LoRa 32 V4 — ESP32-S3R2 + SX1262 + external FEM (GC1109 / KCT8103L)
// LoRa SPI/control pins are identical to V3; FEM adds 3 extra GPIOs
#define RADIO_SCLK_PIN               9
#define RADIO_MISO_PIN              11
#define RADIO_MOSI_PIN              10
#define RADIO_CS_PIN                 8
#define RADIO_DIO1_PIN              14   // IRQ
#define RADIO_RST_PIN               12
#define RADIO_BUSY_PIN              13
// FEM (GC1109) control — required for antenna path; verified against RNode firmware
#define RADIO_VFEM_EN               7    // LORA_PA_PWR_EN: FEM power rail (active HIGH)
#define RADIO_FEM_CE                2    // LORA_PA_CSD:    FEM chip enable  (active HIGH)
#define RADIO_PA_MODE              46    // LORA_PA_CPS:    PA mode HIGH=TX, LOW=RX

#endif

using namespace RNS;

static inline bool    isSplitPacket(uint8_t h)  { return (h & LoRaInterface::HEADER_SPLIT)   != 0; }
static inline uint8_t packetSequence(uint8_t h) { return  h & LoRaInterface::HEADER_SEQ_MASK;      }

/*
@staticmethod
def get_address_for_if(name):
	import RNS.vendor.ifaddr.niwrapper as netinfo
	ifaddr = netinfo.ifaddresses(name)
	return ifaddr[netinfo.AF_INET][0]["addr"]

@staticmethod
def get_broadcast_for_if(name):
	import RNS.vendor.ifaddr.niwrapper as netinfo
	ifaddr = netinfo.ifaddresses(name)
	return ifaddr[netinfo.AF_INET][0]["broadcast"]
*/

LoRaInterface::LoRaInterface(const char* name /*= "LoRaInterface"*/) : RNS::InterfaceImpl(name) {

	_IN = true;
	_OUT = true;
	//p self.bitrate = self.r_sf * ( (4.0/self.r_cr) / (math.pow(2,self.r_sf)/(self.r_bandwidth/1000)) ) * 1000
	// bandwidth is in kHz here (RadioLib units), formula unchanged
	_bitrate = (double)spreading * ( (4.0/coding) / (pow(2, spreading)/bandwidth) ) * 1000.0;
	_HW_MTU = 508;

}

/*virtual*/ LoRaInterface::~LoRaInterface() {
	stop();
}

bool LoRaInterface::start() {
	_online = false;
	INFO("LoRa initializing...");

#ifdef ARDUINO

#if defined(BOARD_TBEAM) || defined(BOARD_LORA32_V21)
	// ESP32: T-Beam and LoRa32 use non-default SPI pins — must specify explicitly
	SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
	_module = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN, SPI);
	SX1276* chip = new SX1276(_module);
	_radio = chip;
	// begin(freq MHz, bw kHz, sf, cr, syncWord, power dBm, preamble symbols, LNA gain 0=AGC)
	int state = chip->begin(frequency, bandwidth, spreading, coding,
	                        RADIOLIB_SX127X_SYNC_WORD, power, 20, 0);

#elif defined(BOARD_RAK4631)
	// nRF52: SPI pins must be configured before SPI.begin()
	SPI.setPins(RADIO_MISO_PIN, RADIO_SCLK_PIN, RADIO_MOSI_PIN);
	SPI.begin();
	// SX1262 Module args: cs, irq=DIO1, rst, busy
	_module = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN, SPI);
	SX1262* chip = new SX1262(_module);
	_radio = chip;
	// DIO2 drives the antenna T/R switch on the RAK4631 SX1262 module
	chip->setDio2AsRfSwitch(true);
	// begin(freq MHz, bw kHz, sf, cr, syncWord, power dBm, preamble symbols,
	//       tcxoVoltage V, useRegulatorLDO)
	// RAK4631 SX1262 module uses a 1.6V TCXO on DIO3; DCDC regulator (LDO=false)
	int state = chip->begin(frequency, bandwidth, spreading, coding,
	                        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, power, 20, 1.6, false);

#elif defined(BOARD_HELTEC_V3)
	// Heltec WiFi LoRa 32 V3 — ESP32-S3 + SX1262, 1.8V TCXO
	SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
	_module = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN, SPI);
	SX1262* chip = new SX1262(_module);
	_radio = chip;
	chip->setDio2AsRfSwitch(true);
	int state = chip->begin(frequency, bandwidth, spreading, coding,
	                        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, power, 20, 1.8, false);

#elif defined(BOARD_HELTEC_V4)
	// Heltec WiFi LoRa 32 V4 — ESP32-S3R2 + SX1262 + external FEM, 1.8V TCXO
	SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
	// Power and enable the FEM — required for antenna path to function at all
	pinMode(RADIO_VFEM_EN, OUTPUT);
	pinMode(RADIO_FEM_CE, OUTPUT);
	pinMode(RADIO_PA_MODE, OUTPUT);
	digitalWrite(RADIO_VFEM_EN, HIGH);
	digitalWrite(RADIO_FEM_CE, HIGH);
	digitalWrite(RADIO_PA_MODE, LOW);   // start in RX mode
	_pa_mode_pin = RADIO_PA_MODE;
	_module = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN, SPI);
	SX1262* chip = new SX1262(_module);
	_radio = chip;
	chip->setDio2AsRfSwitch(true);
	int state = chip->begin(frequency, bandwidth, spreading, coding,
	                        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, power, 20, 1.8, false);

#else
	#error "Unsupported board: define BOARD_TBEAM, BOARD_LORA32_V21, BOARD_RAK4631, BOARD_HELTEC_V3, or BOARD_HELTEC_V4"
	int state = RADIOLIB_ERR_UNKNOWN;
#endif

	if (state != RADIOLIB_ERR_NONE) {
		ERRORF("LoRa init failed, code %d. Check wiring/board define.", state);
		return false;
	}

	// Enter continuous receive mode
	_radio->startReceive();

	INFO("LoRa init succeeded.");
	TRACEF("LoRa bandwidth is %.2f Kbps", Utilities::OS::round(_bitrate/1000.0, 2));
#endif

	_online = true;
	return true;
}

void LoRaInterface::stop() {

#ifdef ARDUINO
	if (_radio) {
		_radio->standby();
	}
#endif

	_online = false;
}

void LoRaInterface::loop() {

	if (_online) {
#ifdef ARDUINO
		// checkIrq() polls the hardware IRQ register — no ISR required
		if (_radio->checkIrq(RADIOLIB_IRQ_RX_DONE)) {
			int len = _radio->getPacketLength();

			uint8_t rxBuf[255];
			int state = _radio->readData(rxBuf, len);

			if (state == RADIOLIB_ERR_NONE && len > 1) {
				Serial.println("RSSI: " + String(_radio->getRSSI()));
				Serial.println("Snr: "  + String(_radio->getSNR()));

				uint8_t hdr = rxBuf[0];
				uint8_t seq = packetSequence(hdr);

				if (isSplitPacket(hdr)) {
					if (_rx_seq == SEQ_UNSET || _rx_seq != seq) {
						// First part of a split (or restart after a lost first part)
						_rx_seq = seq;
						buffer.clear();
						buffer.append(rxBuf + 1, len - 1);
					} else {
						// Second part — sequence matches; assemble and deliver
						buffer.append(rxBuf + 1, len - 1);
						_rx_seq = SEQ_UNSET;
						on_incoming(buffer);
					}
				} else {
					// Non-split: discard any stale partial reassembly, deliver immediately
					if (_rx_seq != SEQ_UNSET) {
						buffer.clear();
						_rx_seq = SEQ_UNSET;
					}
					buffer.clear();
					buffer.append(rxBuf + 1, len - 1);
					on_incoming(buffer);
				}
			} else if (state != RADIOLIB_ERR_NONE) {
				DEBUGF("LoRaInterface: readData failed, code %d", state);
			}

			// Re-arm receive mode (required after every packet on SX1262;
			// harmless on SX1276)
			_radio->startReceive();
		}
#endif
	}
}

/*virtual*/ void LoRaInterface::send_outgoing(const Bytes& data) {
	DEBUGF("%s.on_outgoing: data: %s", toString().c_str(), data.toHex().c_str());
	try {
		if (_online) {
			TRACEF("LoRaInterface: sending %lu bytes...", data.size());
#ifdef ARDUINO
			uint8_t txBuf[255];
			uint8_t rand_nibble = (uint8_t)(Cryptography::randomnum(256)) & 0xF0;

			if ((int)data.size() <= LORA_MAX_PAYLOAD) {
				// Single-frame send
				txBuf[0] = rand_nibble;
				memcpy(txBuf + 1, data.data(), data.size());

				// V4: switch FEM to TX mode before transmitting
				if (_pa_mode_pin >= 0) { digitalWrite(_pa_mode_pin, HIGH); }
				int state = _radio->transmit(txBuf, 1 + data.size());
				// V4: return FEM to RX mode, then re-arm receive
				if (_pa_mode_pin >= 0) { digitalWrite(_pa_mode_pin, LOW); }
				if (state != RADIOLIB_ERR_NONE) {
					ERRORF("LoRaInterface: transmit failed, code %d", state);
				}
			} else {
				// Split send — two frames with matching sequence number
				uint8_t seq       = (_tx_seq_ctr++) & HEADER_SEQ_MASK;
				uint8_t split_hdr = rand_nibble | HEADER_SPLIT | seq;

				// Frame 1: first LORA_MAX_PAYLOAD bytes
				txBuf[0] = split_hdr;
				memcpy(txBuf + 1, data.data(), LORA_MAX_PAYLOAD);

				// V4: switch FEM to TX mode before transmitting
				if (_pa_mode_pin >= 0) { digitalWrite(_pa_mode_pin, HIGH); }
				int state = _radio->transmit(txBuf, 1 + LORA_MAX_PAYLOAD);
				// V4: return FEM to RX mode, then re-arm receive
				if (_pa_mode_pin >= 0) { digitalWrite(_pa_mode_pin, LOW); }
				if (state != RADIOLIB_ERR_NONE) {
					ERRORF("LoRaInterface: transmit part 1 failed, code %d", state);
				}

				// Frame 2: remaining bytes
				size_t remainder = data.size() - LORA_MAX_PAYLOAD;
				txBuf[0] = split_hdr;
				memcpy(txBuf + 1, data.data() + LORA_MAX_PAYLOAD, remainder);

				// V4: switch FEM to TX mode before transmitting
				if (_pa_mode_pin >= 0) { digitalWrite(_pa_mode_pin, HIGH); }
				state = _radio->transmit(txBuf, 1 + remainder);
				// V4: return FEM to RX mode, then re-arm receive
				if (_pa_mode_pin >= 0) { digitalWrite(_pa_mode_pin, LOW); }
				if (state != RADIOLIB_ERR_NONE) {
					ERRORF("LoRaInterface: transmit part 2 failed, code %d", state);
				}
			}

			_radio->startReceive();
#endif
			TRACE("LoRaInterface: sent bytes");
		}

		// Perform post-send housekeeping
		InterfaceImpl::handle_outgoing(data);
	}
	catch (const std::exception& e) {
		ERRORF("Could not transmit on %s. The contained exception was: %s", toString().c_str(), e.what());
	}
}

/*virtual*/ void LoRaInterface::on_incoming(const Bytes& data) {
	DEBUGF("%s.on_incoming: data: %s", toString().c_str(), data.toHex().c_str());
	// Pass received data on to transport
	InterfaceImpl::handle_incoming(data);
}
