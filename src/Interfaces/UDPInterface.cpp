#include "UDPInterface.h"

#include "../Log.h"

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

//p def __init__(self, owner, name, device=None, bindip=None, bindport=None, forwardip=None, forwardport=None):
UDPInterface::UDPInterface(const char* name /*= "UDPInterface"*/) : Interface(name) {

	IN(true);
	//OUT(false);
	OUT(true);
	bitrate(BITRATE_GUESS);

}

void UDPInterface::start() {
  
#ifdef ARDUINO
	// Connect to the WiFi network
	WiFi.begin(ssid, pwd);
	Serial.println("");

	// Wait for WiFi network connection to complete
	Serial.print("Connecting to ");
	Serial.print(ssid);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// Listen for received UDP packets
/*
	if (udp.listen(udpPort)) {
		Serial.print("UDP Listening on IP: ");
		Serial.println(WiFi.localIP());
		udp.onPacket([](AsyncUDPPacket packet) {
			Serial.print("UDP Packet Type: ");
			Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
			Serial.print(", From: ");
			Serial.print(packet.remoteIP());
			Serial.print(":");
			Serial.print(packet.remotePort());
			Serial.print(", To: ");
			Serial.print(packet.localIP());
			Serial.print(":");
			Serial.print(packet.localPort());
			Serial.print(", Length: ");
			Serial.print(packet.length()); //dlzka packetu
			Serial.print(", Data: ");
			Serial.write(packet.data(), packet.length());
			Serial.println();
			String myString = (const char*)packet.data();
			if (myString == "ZAP") {
			Serial.println("Zapinam rele");
			digitalWrite(rele, LOW);
			} else if (myString == "VYP") {
			Serial.println("Vypinam rele");
			digitalWrite(rele, HIGH);
			}
			packet.printf("Got %u bytes of data", packet.length());
		});
	}
*/

	// This initializes udp and transfer buffer
	udp.begin(udpPort);
#endif

	online(true);
}

void UDPInterface::loop() {

	if (online()) {
		// Check for incoming packet
#ifdef ARDUINO
		udp.parsePacket();
		size_t len = udp.read(buffer.writable(Type::Reticulum::MTU), Type::Reticulum::MTU);
		if (len > 0) {
			buffer.resize(len);
			processIncoming(buffer);
		}
#endif
	}
}

/*virtual*/ void UDPInterface::processIncoming(const Bytes& data) {
	debug("UDPInterface.processIncoming: data: " + data.toHex());
	//debug("UDPInterface.processIncoming: text: " + data.toString());
	Interface::processIncoming(data);
}

/*virtual*/ void UDPInterface::processOutgoing(const Bytes& data) {
	debug("UDPInterface.processOutgoing: data: " + data.toHex());
	debug("UDPInterface.processOutgoing: text: " + data.toString());
	try {
		if (online()) {
			// Send packet
#ifdef ARDUINO
			udp.beginPacket(udpAddress, udpPort);
			udp.write(data.data(), data.size());
			udp.endPacket();
#endif
		}

		Interface::processOutgoing(data);
	}
	catch (std::exception& e) {
		error("Could not transmit on " + toString() + ". The contained exception was: " + e.what());
	}
}
