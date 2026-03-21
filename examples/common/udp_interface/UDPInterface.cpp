#include "UDPInterface.h"

#include <Transport.h>
#include "../src/Log.h"

#include <memory>

#ifndef ARDUINO
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
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

//p def __init__(self, owner, name, device=None, bindip=None, bindport=None, forwardip=None, forwardport=None):
UDPInterface::UDPInterface(const char* name /*= "UDPInterface"*/) : RNS::InterfaceImpl(name) {

	_IN = true;
	_OUT = true;
	_bitrate = BITRATE_GUESS;
	_HW_MTU = 1064;

}

/*virtual*/ UDPInterface::~UDPInterface() {
	stop();
}

//bool UDPInterface::start(const char* wifi_ssid, const char* wifi_password, int port /*= DEFAULT_UDP_PORT*/, const char* local_host /*= nullptr*/) {
/*virtual*/ bool UDPInterface::start() {
	const char* wifi_ssid = "wifi_ssid";
	const char* wifi_password = "wifi_password";
	int port = DEFAULT_UDP_PORT;
	const char* local_host = nullptr;

	_online = false;
 
	if (wifi_ssid != nullptr) {
		_wifi_ssid = wifi_ssid;
	}
	if (wifi_password != nullptr) {
		_wifi_password = wifi_password;
	}
	if (local_host != nullptr) {
		_local_host = local_host;
	}
	_local_port = port;
	_remote_port = port;
	TRACEF("UDPInterface: local host: %s", _local_host.c_str());
	TRACEF("UDPInterface: local port: %d", _local_port);
	TRACEF("UDPInterface: remote host: %s", _remote_host.c_str());
	TRACEF("UDPInterface: remote port: %d", _remote_port);
 
#ifdef ARDUINO
	TRACEF("UDPInterface: wifi ssid: %s", _wifi_ssid.c_str());
	TRACEF("UDPInterface: wifi password: %s", _wifi_password.c_str());

	// Connect to the WiFi network
	WiFi.begin(_wifi_ssid.c_str(), _wifi_password.c_str());
	Serial.println("");

	// Wait for WiFi network connection to complete
	Serial.print("Connecting to ");
	Serial.print(_wifi_ssid.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(_wifi_ssid.c_str());
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
	udp.begin(_local_port);
#else

	// resolve local host
	struct in_addr local_addr;
	if (inet_aton(_local_host.c_str(), &local_addr) == 0) {
		struct hostent* host_ent = gethostbyname(_local_host.c_str());
		if (host_ent == nullptr || host_ent->h_addr_list[0] == nullptr) {
			ERRORF("Unable to resolve local host %s", _local_host.c_str());
			return false;
		}
		_local_address = *((in_addr_t*)(host_ent->h_addr_list[0]));
	}
	else {
		_local_address = local_addr.s_addr;
	}

	// resolve remote host
	struct in_addr remote_addr;
	if (inet_aton(_remote_host.c_str(), &remote_addr) == 0) {
		struct hostent* host_ent = gethostbyname(_remote_host.c_str());
		if (host_ent == nullptr || host_ent->h_addr_list[0] == nullptr) {
			ERRORF("Unable to resolve remote host %s", _remote_host.c_str());
			return false;
		}
		_remote_address = *((in_addr_t*)(host_ent->h_addr_list[0]));
	}
	else {
		_remote_address = remote_addr.s_addr;
	}

	// create udp socket
	TRACE("Opening UDP socket");
	_socket = socket( PF_INET, SOCK_DGRAM, 0 );
	if (_socket < 0) {
		ERRORF("Unable to create socket with error %d", errno);
		return false;
	}

	// enable broadcast
	int broadcast = 1;
	setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

	// enable ip/port sharing
    int reuse = 1;
    setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
	setsockopt(_socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

	// bind to interface for listening
	INFOF("Binding UDP socket %d to %s:%d", _socket, _local_host.c_str(), _local_port);
	sockaddr_in bind_addr;
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = _local_address;
	bind_addr.sin_port = htons(_local_port);
	if (bind(_socket, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == -1) {
		close(_socket);
		_socket = -1;
		ERRORF("Unable to bind socket with error %d", errno);
		return false;
	}
#endif

	_online = true;

	return true;
}

/*virtual*/ void UDPInterface::stop() {
#ifdef ARDUINO
#else
	if (_socket > -1) {
		close(_socket);
		_socket = -1;
	}
#endif

	_online = false;
}

/*virtual*/ void UDPInterface::loop() {

	if (_online) {
		// Check for incoming packet
#ifdef ARDUINO
		udp.parsePacket();
		size_t len = udp.read(_buffer.writable(Type::Reticulum::MTU), Type::Reticulum::MTU);
		if (len > 0) {
			_buffer.resize(len);
			on_incoming(_buffer);
		}
#else
		size_t available = 0;
		ioctl(_socket, FIONREAD, &available);
		if (available > 0) {
			size_t len = read(_socket, _buffer.writable(available), available);
			if (len > 0) {
				if (len < available) {
					_buffer.resize(len);
				}
				on_incoming(_buffer);
			}
		}
#endif
	}
}

/*virtual*/ void UDPInterface::send_outgoing(const Bytes& data) {
	DEBUGF("%s.on_outgoing: data: %s", toString().c_str(), data.toHex().c_str());
	try {
		if (_online) {
			// Send packet
#ifdef ARDUINO
			udp.beginPacket(_remote_host.c_str(), _remote_port);
			udp.write(data.data(), data.size());
			udp.endPacket();
#else
			TRACEF("Sending UDP packet to %s:%d", _remote_host.c_str(), _remote_port);
			sockaddr_in sock_addr;
			sock_addr.sin_family = AF_INET;
			sock_addr.sin_addr.s_addr = _remote_address;
			sock_addr.sin_port = htons(_remote_port);
			int sent = sendto(_socket, data.data(), data.size(), 0, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
			TRACEF("Sent %d bytes to %s:%d", sent, _remote_host.c_str(), _remote_port);
#endif
		}

		// Perform post-send housekeeping
		InterfaceImpl::handle_outgoing(data);
	}
	catch (const std::exception& e) {
		ERRORF("Could not transmit on %s. The contained exception was: %s", toString().c_str(), e.what());
	}
}

void UDPInterface::on_incoming(const Bytes& data) {
	DEBUGF("%s.on_incoming: data: %s", toString().c_str(), data.toHex().c_str());
	// Pass received data on to transport
	InterfaceImpl::handle_incoming(data);
}
