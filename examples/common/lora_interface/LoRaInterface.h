#pragma once

#include "../src/Interface.h"
#include "../src/Bytes.h"
#include "../src/Type.h"

#ifdef ARDUINO
#include <SPI.h>
#include <RadioLib.h>
#include "LoRaCSMA.h"
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
	void do_transmit();   // performs the actual radio transmit after CSMA clears it

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
	SX127x*        _radio_127x  = nullptr;   // non-null for SX127x boards; used by CSMA
	int            _pa_mode_pin = -1;        // V4 FEM PA mode pin; -1 = not present

	// CSMA/CCA engine
	LoRaCSMA       _csma;

	// Single-packet TX queue: send_outgoing() enqueues here;
	// do_transmit() is called by loop() once CSMA grants access.
	RNS::Bytes     _tx_pending;
	bool           _tx_pending_valid = false;

	// Single-packet RX queue: assembled packet is enqueued here after
	// readData(); on_incoming() is called by loop() on the next iteration
	// so the radio can be re-armed immediately after the read.
	RNS::Bytes     _rx_pending;
	bool           _rx_pending_valid = false;

	// startReceive() IRQ flags: default set + preamble-detected latch
	// (preamble-detected is needed by the SX126x DCD polling logic)
	static constexpr RadioLibIrqFlags_t CSMA_RX_FLAGS =
	    RADIOLIB_IRQ_RX_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED);
#endif

};
