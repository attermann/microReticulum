#pragma once

#include "../src/Interface.h"
#include "../src/Bytes.h"
#include "../src/Type.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <WiFiUdp.h>
//#include <AsyncUDP.h>
#else
#include <netinet/in.h>
#endif

#include <stdint.h>

#define DEFAULT_UDP_PORT		4242
#define DEFAULT_UDP_LOCAL_HOST	"0.0.0.0"
#define DEFAULT_UDP_REMOTE_HOST	"255.255.255.255"

class UDPInterface : public RNS::InterfaceImpl {

public:
	static const uint32_t BITRATE_GUESS = 10*1000*1000;

	//z def get_address_for_if(name):
	//z def get_broadcast_for_if(name):

public:
	//p def __init__(self, owner, name, device=None, bindip=None, bindport=None, forwardip=None, forwardport=None):
	UDPInterface(const char* name = "UDPInterface");
	virtual ~UDPInterface();

	//bool start(const char* wifi_ssid, const char* wifi_password, int port = DEFAULT_UDP_PORT, const char* local_host = nullptr);
	virtual bool start();
	virtual void stop();
	virtual void loop();

	virtual inline std::string toString() const { return "UDPInterface[" + _name + "/" + _local_host + ":" + std::to_string(_local_port) + "]"; }
	//virtual inline std::string toString() const { return "UDPInterface[" + name() + "]"; }

protected:
	virtual void send_outgoing(const RNS::Bytes& data);
	void on_incoming(const RNS::Bytes& data);

private:
	//uint8_t buffer[Type::Reticulum::MTU] = {0};
	RNS::Bytes _buffer;

	// WiFi network name and password
	std::string _wifi_ssid;
	std::string _wifi_password;

	// IP address to send UDP data to.
	// it can be ip address of the server or 
	// broadcast
	std::string _local_host = DEFAULT_UDP_LOCAL_HOST;
	int _local_port = DEFAULT_UDP_PORT;
	std::string _remote_host = DEFAULT_UDP_REMOTE_HOST;
	int _remote_port = DEFAULT_UDP_PORT;

	// create UDP instance
#ifdef ARDUINO
	WiFiUDP udp;
	//AsyncUDP udp;
#else
	int _socket = -1;
	in_addr_t _local_address = INADDR_ANY;
	in_addr_t _remote_address = INADDR_NONE;
#endif

};
