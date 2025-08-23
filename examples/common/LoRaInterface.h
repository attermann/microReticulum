#pragma once

#include "../src/Interface.h"
#include "../src/Bytes.h"
#include "../src/Type.h"

#ifdef ARDUINO
#include <SPI.h>
#include <LoRa.h>
#endif

// LILYGO T-Beam V1.X
#define RADIO_SCLK_PIN               5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN              26
#define RADIO_RST_PIN               23
#define RADIO_DIO1_PIN              33
#define RADIO_BUSY_PIN              32

#include <stdint.h>

class LoRaInterface : public RNS::InterfaceImpl {

public:
	//z def get_address_for_if(name):
	//z def get_broadcast_for_if(name):

public:
	//p def __init__(self, owner, name, device=None, bindip=None, bindport=None, forwardip=None, forwardport=None):
	LoRaInterface(const char* name = "LoRaInterface");
	virtual ~LoRaInterface();

	virtual bool start();
	virtual void stop();
	virtual void loop();

	//virtual inline std::string toString() const { return "LoRaInterface[" + name() + "]"; }

private:
	virtual void send_outgoing(const RNS::Bytes& data);
	void on_incoming(const RNS::Bytes& data);

private:
	//uint8_t buffer[Type::Reticulum::MTU] = {0};
	const uint8_t message_count = 0;
	RNS::Bytes buffer;

	const long frequency = 915E6;

	// Reticulum default
	const long bandwidth = 125E3;
	const int spreading = 8;
	const int coding = 5;
	const int power = 17;

};
