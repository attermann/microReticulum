#pragma once

#include "../src/Interface.h"
#include "../src/Bytes.h"
#include "../src/Type.h"

#ifdef ARDUINO
#include <SPI.h>
#include <RadioLib.h>
#endif

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

	// Split-packet protocol constants
	static constexpr uint8_t HEADER_SPLIT     = 0x08;  // bit 3: split-packet flag
	static constexpr uint8_t HEADER_SEQ_MASK  = 0x07;  // bits 2:0: sequence number
	static constexpr uint8_t SEQ_UNSET        = 0xFF;  // sentinel: no split in progress
	static constexpr int     LORA_MAX_PAYLOAD = 254;   // 255 - 1 header byte

	uint8_t _rx_seq     = SEQ_UNSET;  // sequence of split RX in progress
	uint8_t _tx_seq_ctr = 0;          // rolling TX split sequence counter

	// Radio parameters (RadioLib units: MHz, kHz)
	const float frequency = 915.0;   // MHz
	const float bandwidth = 125.0;   // kHz
	const int   spreading = 8;
	const int   coding    = 5;
	const int   power     = 17;      // dBm

#ifdef ARDUINO
	Module*        _module      = nullptr;
	PhysicalLayer* _radio       = nullptr;
	int            _pa_mode_pin = -1;    // V4 FEM PA mode pin; -1 = not present
#endif

};
