#include "LoRaInterface.h"

#include "../Log.h"
#include "../Utilities/OS.h"

#include <memory>

using namespace RNS;
using namespace RNS::Interfaces;

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

LoRaInterface::LoRaInterface(const char* name /*= "LoRaInterface"*/) : Interface(name) {

	IN(true);
	OUT(true);
	//p self.bitrate = self.r_sf * ( (4.0/self.r_cr) / (math.pow(2,self.r_sf)/(self.r_bandwidth/1000)) ) * 1000
	bitrate((double)spreading * ( (4.0/coding) / (pow(2, spreading)/(bandwidth/1000.0)) ) * 1000.0);

}

/*virtual*/ LoRaInterface::~LoRaInterface() {
	stop();
}

bool LoRaInterface::start() {
	online(false);
	info("LoRa initializing...");
  
#ifdef ARDUINO
	SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
	delay(1500);

    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

	// initialize radio
	if (!LoRa.begin(frequency)) {
		error("LoRa init failed. Check your connections.");
		return false;
	}

	LoRa.setSignalBandwidth(bandwidth);
	LoRa.setSpreadingFactor(spreading);
	LoRa.setCodingRate4(coding);
	LoRa.setPreambleLength(20);
	LoRa.setTxPower(power);

	info("LoRa init succeeded.");
	extreme("LoRa bandwidth is " + std::to_string(Utilities::OS::round(bitrate()/1000.0, 2)) + " Kbps");
#endif

	online(true);
	return true;
}

void LoRaInterface::stop() {

#ifdef ARDUINO
#endif

	online(false);
}

void LoRaInterface::loop() {

	if (online()) {
		// Check for incoming packet
#ifdef ARDUINO
		int available = LoRa.parsePacket();
		if (available > 0) {
Serial.println(available);
			extreme("LoRaInterface: receiving bytes...");

			// read packet
			buffer.clear();
			while (LoRa.available()) {
				buffer << (uint8_t)LoRa.read();
			}

			Serial.println("RSSI: " + String(LoRa.packetRssi()));
			Serial.println("Snr: " + String(LoRa.packetSnr()));

			processIncoming(buffer);
		}
#endif
	}
}

/*virtual*/ void LoRaInterface::processIncoming(const Bytes& data) {
	debug("LoRaInterface.processIncoming: data: " + data.toHex());
	Interface::processIncoming(data);
}

/*virtual*/ void LoRaInterface::processOutgoing(const Bytes& data) {
	debug("LoRaInterface.processOutgoing: data: " + data.toHex());
	try {
		if (online()) {
			extreme("LoRaInterface: sending " + std::to_string(data.size()) + " bytes...");
			// Send packet
#ifdef ARDUINO

			LoRa.beginPacket();                   // start packet

			// add payload
			//LoRa.print((const char*)data.data());
			for (size_t i = 0; i < data.size(); ++i) {
				LoRa.write(data.data()[i]);
			}

			LoRa.endPacket();                     // finish packet and send it

#endif
			extreme("LoRaInterface: sent bytes");
		}
		Interface::processOutgoing(data);
	}
	catch (std::exception& e) {
		error("Could not transmit on " + toString() + ". The contained exception was: " + e.what());
	}
}
