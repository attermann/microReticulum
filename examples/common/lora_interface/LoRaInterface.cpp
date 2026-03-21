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

#endif

using namespace RNS;

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

#else
	#error "Unsupported board: define BOARD_TBEAM, BOARD_LORA32_V21, or BOARD_RAK4631"
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

			// Buffer sized for max HW_MTU (508) + 1 header byte
			uint8_t rxBuf[509];
			int state = _radio->readData(rxBuf, len);

			if (state == RADIOLIB_ERR_NONE && len > 1) {
				Serial.println("RSSI: " + String(_radio->getRSSI()));
				Serial.println("Snr: "  + String(_radio->getSNR()));

				// Skip the 1-byte split-packet header, forward payload
				buffer.clear();
				for (int i = 1; i < len; i++) {
					buffer << rxBuf[i];
				}
				on_incoming(buffer);
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
			// Prepend 1-byte header (high nibble random, reserved for split-packet support)
			// CBA TODO add support for split packets
			uint8_t txBuf[509];
			txBuf[0] = Cryptography::randomnum(256) & 0xF0;
			memcpy(txBuf + 1, data.data(), data.size());

			int state = _radio->transmit(txBuf, 1 + data.size());
			if (state != RADIOLIB_ERR_NONE) {
				ERRORF("LoRaInterface: transmit failed, code %d", state);
			}

			// Return to receive mode after blocking transmit
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
