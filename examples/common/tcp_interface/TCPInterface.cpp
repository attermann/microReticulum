#include "TCPInterface.h"

#include "../src/Log.h"
#include <Transport.h>

#ifndef ARDUINO
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
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

// p def __init__(self, owner, name, device=None, bindip=None, bindport=None,
// forwardip=None, forwardport=None):
TCPInterface::TCPInterface(const char *name /*= "TCPInterface"*/)
    : RNS::InterfaceImpl(name) {

  _IN = true;
  _OUT = true;
  _bitrate = BITRATE_GUESS;
  _HW_MTU = 1064;
}

/*virtual*/ TCPInterface::~TCPInterface() { stop(); }

// bool TCPInterface::start(const char* wifi_ssid, const char* wifi_password,
// int port /*= DEFAULT_TCP_PORT*/, const char* local_host /*= nullptr*/) {
/*virtual*/ bool TCPInterface::start() {
  const char *wifi_ssid = "wifi_ssid";
  const char *wifi_password = "wifi_password";
  int port = DEFAULT_TCP_LISTEN_PORT;
  const char *listen_host = nullptr;

  _online = false;

  if (wifi_ssid != nullptr) {
    _wifi_ssid = wifi_ssid;
  }
  if (wifi_password != nullptr) {
    _wifi_password = wifi_password;
  }
  if (listen_host != nullptr) {
    _listen_host = listen_host;
  }
  _listen_port = port;
  TRACE("TCPInterface: listen host: " + _listen_host);
  TRACE("TCPInterface: listen port: " + std::to_string(_listen_port));

#ifdef ARDUINO
  // TODO: ALL THIS STUFF
  TRACE("TCPInterface: wifi ssid: " + _wifi_ssid);
  TRACE("TCPInterface: wifi password: " + _wifi_password);

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

  // This initializes tcp server and transfer buffer
  server.begin(_listen_port);
#else

  struct in_addr local_addr;
  if (inet_aton(_listen_host.c_str(), &local_addr) == 0) {
    struct hostent *host_ent = gethostbyname(_listen_host.c_str());
    if (host_ent == nullptr || host_ent->h_addr_list[0] == nullptr) {
      ERROR("Unable to resolve local host " + std::string(listen_host));
      return false;
    }
    _local_address = *((in_addr_t *)(host_ent->h_addr_list[0]));
  } else {
    _local_address = local_addr.s_addr;
  }

  TRACE("Opening TCP socket");
  _socket = socket(PF_INET, SOCK_STREAM, 0);
  if (_socket < 0) {
    ERROR("Unable to create socket with error " + std::to_string(errno));
    return false;
  }

  int reuse = 1;
  setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
  setsockopt(_socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

  INFO("Binding TCP socket " + std::to_string(_socket) + " to " + _listen_host +
       ":" + std::to_string(_listen_port));
  sockaddr_in bind_addr;
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = _local_address;
  bind_addr.sin_port = htons(_listen_port);
  if (bind(_socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1) {
    close(_socket);
    _socket = -1;
    ERROR("Unable to bind socket with error " + std::to_string(errno));
    return false;
  }

  if (listen(_socket, 5) == -1) {
    close(_socket);
    _socket = -1;
    ERROR("Unable to listen on socket with error " + std::to_string(errno));
    return false;
  }
  INFO("TCP socket listening on " + _listen_host + ":" +
       std::to_string(_listen_port));

  int flags = fcntl(_socket, F_GETFL, 0);
  fcntl(_socket, F_SETFL, flags | O_NONBLOCK);
#endif

  _online = true;

  return true;
}

/*virtual*/ void TCPInterface::stop() {
#ifdef ARDUINO
#else
  // Close all client connections
  for (int client_socket : _client_sockets) {
    close(client_socket);
  }
  _client_sockets.clear();
  _client_states.clear();

  // Close server socket
  if (_socket > -1) {
    close(_socket);
    _socket = -1;
  }
#endif

  _online = false;
}

/*virtual*/ void TCPInterface::loop() {

  if (_online) {
#ifdef ARDUINO
  // Check for incoming clients
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New client connected!");
    _clients.push_back(client);
    // Loop while the client is connected
    while (client.connected()) {
      // Check if there is data available to read
      if (client.available()) {
        String data = client.readStringUntil('\n'); // Read until newline character
        Serial.print("Received from client: ");
        Serial.println(data);

        // Send a response back to the client
        client.println("Message received!");
      }
    }
    // Close the connection when the client disconnects
    client.stop();
    Serial.println("Client disconnected.");
  }
#else
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_client =
        accept(_socket, (struct sockaddr *)&client_addr, &client_len);

    if (new_client > 0) {
      int flags = fcntl(new_client, F_GETFL, 0);
      fcntl(new_client, F_SETFL, flags | O_NONBLOCK);

      _client_sockets.push_back(new_client);
      _client_states[new_client] = ClientState();
      INFO("New client connected: socket " + std::to_string(new_client) +
           " from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" +
           std::to_string(ntohs(client_addr.sin_port)));
    }

    for (auto it = _client_sockets.begin(); it != _client_sockets.end();) {
      int client_socket = *it;
      size_t available = 0;

      if (ioctl(client_socket, FIONREAD, &available) == 0 && available > 0) {
        RNS::Bytes temp_buffer;
        ssize_t len =
            read(client_socket, temp_buffer.writable(available), available);
        if (len > 0) {
          temp_buffer.resize(len);
          _client_states[client_socket].buffer.append(temp_buffer);
          process_frames(client_socket);
          ++it;
        } else if (len == 0) {
          INFO("Client disconnected: socket " + std::to_string(client_socket));
          close(client_socket);
          _client_states.erase(client_socket);
          it = _client_sockets.erase(it);
        } else {
          ERROR("Read error on client socket " + std::to_string(client_socket) +
                ": " + std::to_string(errno));
          close(client_socket);
          _client_states.erase(client_socket);
          it = _client_sockets.erase(it);
        }
      } else {
        ++it;
      }
    }
#endif
  }
}

/*virtual*/ void TCPInterface::send_outgoing(const Bytes &data) {
  DEBUG(toString() + ".on_outgoing: data: " + data.toHex());
  try {
    if (_online) {
#ifdef ARDUINO
      // TODO: Arduino TCP implementation
#else
      RNS::Bytes hdlc_frame = hdlc_encode(data);

      for (auto it = _client_sockets.begin(); it != _client_sockets.end();) {
        int client_socket = *it;
        ssize_t sent = send(client_socket, hdlc_frame.data(), hdlc_frame.size(),
                            MSG_NOSIGNAL);

        if (sent > 0) {
          TRACE("Sent " + std::to_string(sent) + " bytes to client socket " +
                std::to_string(client_socket));
          ++it;
        } else if (sent == 0 || (sent < 0 && (errno == EPIPE || errno == ECONNRESET))) {
          INFO("Client disconnected during send: socket " +
               std::to_string(client_socket));
          close(client_socket);
          _client_states.erase(client_socket);
          it = _client_sockets.erase(it);
        } else {
          ERROR("Send error on client socket " + std::to_string(client_socket) +
                ": " + std::to_string(errno));
          ++it;
        }
      }
#endif
    }

    InterfaceImpl::handle_outgoing(data);
  } catch (std::exception &e) {
    ERROR("Could not transmit on " + toString() +
          ". The contained exception was: " + e.what());
  }
}

void TCPInterface::process_frames(int client_socket) {
  ClientState &state = _client_states[client_socket];
  hdlc_decode_stream(state.buffer, client_socket);
}

RNS::Bytes TCPInterface::hdlc_encode(const RNS::Bytes &data) {
  DEBUG("HDLC encode input: " + data.toHex());

  std::vector<uint8_t> buffer;
  buffer.reserve(data.size() * 2 + 2);

  buffer.push_back(0x7E);

  for (size_t i = 0; i < data.size(); i++) {
    uint8_t byte = data[i];
    if (byte == 0x7E || byte == 0x7D) {
      uint8_t escaped_byte = byte ^ 0x20;
      DEBUG("HDLC escape: " + RNS::Bytes(&byte, 1).toHex() + " -> 7d" +
            RNS::Bytes(&escaped_byte, 1).toHex());
      buffer.push_back(0x7D);        // Escape byte
      buffer.push_back(byte ^ 0x20); // Escaped data
    } else {
      buffer.push_back(byte); // Normal data
    }
  }

  buffer.push_back(0x7E);

  RNS::Bytes encoded;
  encoded.resize(buffer.size());
  memcpy(encoded.writable(buffer.size()), buffer.data(), buffer.size());

  DEBUG("HDLC encode output: " + encoded.toHex());
  return encoded;
}

void TCPInterface::hdlc_decode_stream(RNS::Bytes &buffer, int client_socket) {
  ClientState &state = _client_states[client_socket];

  DEBUG("HDLC decode stream input: " + buffer.toHex());

  for (size_t i = 0; i < buffer.size(); i++) {
    uint8_t byte = buffer[i];

    if (byte == HDLC_FLAG) {
      if (state.in_frame && state.frame_data.size() > 0) {
        DEBUG("HDLC decode output: " + state.frame_data.toHex());
        on_incoming(state.frame_data);
        state.frame_data.clear();
      }
      state.in_frame = true;
      state.escape_next = false;
    } else if (state.in_frame) {
      if (state.escape_next) {
        uint8_t decoded_byte = byte ^ HDLC_ESC_MASK;
        DEBUG("HDLC unescape: " + RNS::Bytes(&byte, 1).toHex() + " -> " + RNS::Bytes(&decoded_byte, 1).toHex());
        state.frame_data.append(decoded_byte);
        state.escape_next = false;
      } else if (byte == HDLC_ESC) {
        DEBUG("HDLC escape found, next byte will be unescaped");
        state.escape_next = true;
      } else {
        state.frame_data.append(byte);
      }
    }
  }

  buffer.clear();
}

void TCPInterface::on_incoming(const Bytes &data) {
  DEBUG(toString() + ".on_incoming: data: " + data.toHex());
  InterfaceImpl::handle_incoming(data);
}
