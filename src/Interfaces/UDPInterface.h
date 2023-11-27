#pragma once

#include "../Interface.h"
#include "../Bytes.h"
#include "../Type.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <WiFiUdp.h>
//#include <AsyncUDP.h>
#endif

#include <stdint.h>

namespace RNS { namespace Interfaces {

    class UDPInterface : public Interface {

	public:
		static const uint32_t BITRATE_GUESS = 10*1000*1000;

		//z def get_address_for_if(name):
		//z def get_broadcast_for_if(name):

	public:
		//p def __init__(self, owner, name, device=None, bindip=None, bindport=None, forwardip=None, forwardport=None):
		UDPInterface(const char* name = "UDPInterface");

		void start();
		void loop();

	    virtual void processIncoming(const Bytes& data);
		virtual void processOutgoing(const Bytes& data);

		//virtual inline std::string toString() const { return "UDPInterface[" + name() + "/" + bind_ip + ":" + bind_port + "]"; }
		virtual inline std::string toString() const { return "UDPInterface[" + name() + "]"; }

	private:
		const uint16_t HW_MTU = 1064;
		//uint8_t buffer[Type::Reticulum::MTU] = {0};
		Bytes buffer;

		// WiFi network name and password
		const char* ssid = "some-ssid";
		const char* pwd = "some-pass";

		// IP address to send UDP data to.
		// it can be ip address of the server or 
		// broadcast
		const char* udpAddress = "255.255.255.255";
		const int udpPort = 4242;

		// create UDP instance
#ifdef ARDUINO
		WiFiUDP udp;
		//AsyncUDP udp;
#endif

	};

} }
