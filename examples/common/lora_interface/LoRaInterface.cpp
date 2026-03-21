#include "LoRaInterface.h"

#include "../src/Log.h"
#include "../src/Utilities/OS.h"

#include <memory>

#if defined(BOARD_TBEAM) || defined(BOARD_LOR32_V21)
// LILYGO T-Beam V1.X
#define RADIO_SCLK_PIN               5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN              26
#define RADIO_RST_PIN               23
#define RADIO_DIO1_PIN              33
#define RADIO_BUSY_PIN              32
#elif defined(BOARD_RAK4631)
#define RADIO_SCLK_PIN              43
#define RADIO_MISO_PIN              45
#define RADIO_MOSI_PIN              44
#define RADIO_CS_PIN                42
#define RADIO_DIO0_PIN              47
#define RADIO_RST_PIN               38
#define RADIO_DIO1_PIN              -1
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
	_bitrate = (double)spreading * ( (4.0/coding) / (pow(2, spreading)/(bandwidth/1000.0)) ) * 1000.0;
	_HW_MTU = 508;

}

/*virtual*/ LoRaInterface::~LoRaInterface() {
	stop();
}

bool LoRaInterface::start() {
	_online = false;
	INFO("LoRa initializing...");
  
#ifdef ARDUINO

#if defined(BOARD_NRF52) || defined(MCU_NRF52)
	SPI.setPins(RADIO_MISO_PIN, RADIO_SCLK_PIN, RADIO_MOSI_PIN);
	SPI.begin();
#else
	//SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
	SPI.begin();
#endif

delay(1500);

    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

	// initialize radio
	if (!LoRa.begin(frequency)) {
		ERROR("LoRa init failed. Check your connections.");
		return false;
	}

	LoRa.setSignalBandwidth(bandwidth);
	LoRa.setSpreadingFactor(spreading);
	LoRa.setCodingRate4(coding);
	LoRa.setPreambleLength(20);
	LoRa.setTxPower(power);

	INFO("LoRa init succeeded.");
	TRACEF("LoRa bandwidth is %.2f Kbps", Utilities::OS::round(_bitrate/1000.0, 2));
#endif

	_online = true;
	return true;
}

void LoRaInterface::stop() {

#ifdef ARDUINO
#endif

	_online = false;
}

void LoRaInterface::loop() {

	if (_online) {
		// Check for incoming packet
#ifdef ARDUINO
		int available = LoRa.parsePacket();
		if (available > 0) {
			TRACE("LoRaInterface: receiving bytes...");

			// read header (for detecting split packets)
		    uint8_t header  = LoRa.read();

			// CBA TODO add support for split packets

			// read packet
			buffer.clear();
			while (LoRa.available()) {
				buffer << (uint8_t)LoRa.read();
			}

			Serial.println("RSSI: " + String(LoRa.packetRssi()));
			Serial.println("Snr: " + String(LoRa.packetSnr()));

			on_incoming(buffer);
		}
#endif
	}
}

/*virtual*/ void LoRaInterface::send_outgoing(const Bytes& data) {
	DEBUGF("%s.on_outgoing: data: %s", toString().c_str(), data.toHex().c_str());
	try {
		if (_online) {
			TRACEF("LoRaInterface: sending %lu bytes...", data.size());
			// Send packet
#ifdef ARDUINO

			LoRa.beginPacket();                   // start packet

			// write header (for detecting split packets)
		    uint8_t header  = Cryptography::randomnum(256) & 0xF0;
			LoRa.write(header);

			// CBA TODO add support for split packets

			// add payload
			//LoRa.print((const char*)data.data());
			for (size_t i = 0; i < data.size(); ++i) {
				LoRa.write(data.data()[i]);
			}

			LoRa.endPacket();                     // finish packet and send it

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
