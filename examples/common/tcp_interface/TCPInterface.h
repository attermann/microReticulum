#pragma once

#include "../src/Bytes.h"
#include "../src/Interface.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#else
#include <netinet/in.h>
#endif

#include <map>
#include <stdint.h>
#include <vector>

#define DEFAULT_TCP_LISTEN_PORT 4242
#define DEFAULT_TCP_LISTEN_IP "0.0.0.0"

class TCPInterface : public RNS::InterfaceImpl {

public:
  static const uint32_t BITRATE_GUESS = 10 * 1000 * 1000;

  // HDLC constants
  static const uint8_t HDLC_FLAG = 0x7E;
  static const uint8_t HDLC_ESC = 0x7D;
  static const uint8_t HDLC_ESC_MASK = 0x20;

public:
  TCPInterface(const char *name = "TCPInterface");
  virtual ~TCPInterface();

  virtual bool start();
  virtual void stop();
  virtual void loop();

  virtual inline std::string toString() const {
    return "TCPInterface[" + _name + "/" + _listen_host + ":" +
           std::to_string(_listen_port) + "]";
  }

protected:
  virtual void send_outgoing(const RNS::Bytes &data);
  void on_incoming(const RNS::Bytes &data);
  void process_frames(int client_socket);

  // HDLC framing methods
  RNS::Bytes hdlc_encode(const RNS::Bytes &data);
  void hdlc_decode_stream(RNS::Bytes &buffer, int client_socket);

private:
  // uint8_t buffer[Type::Reticulum::MTU] = {0};
  RNS::Bytes _buffer;

  // TODO: REMOVE?
  std::string _wifi_ssid;
  std::string _wifi_password;

  std::string _listen_host = DEFAULT_TCP_LISTEN_IP;
  int _listen_port = DEFAULT_TCP_LISTEN_PORT;

#ifdef ARDUINO
  WiFiServer server;
  std::vector<WiFiClient> _clients;
#else
  int _socket = -1;
  in_addr_t _local_address = INADDR_ANY;
  std::vector<int> _client_sockets;
#endif
  struct ClientState {
    RNS::Bytes buffer;
    RNS::Bytes frame_data;
    bool in_frame = false;
    bool escape_next = false;
  };
  std::map<int, ClientState> _client_states;
};
