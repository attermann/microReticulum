// #define NDEBUG

#include <TCPInterface.h>
#include <UniversalFileSystem.h>

#include <Bytes.h>
#include <Destination.h>
#include <Identity.h>
#include <Interface.h>
#include <Log.h>
#include <Packet.h>
#include <Reticulum.h>
#include <Transport.h>
#include <Type.h>
#include <Utilities/OS.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <SPIFFS.h>
#else
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#endif

#include <stdlib.h>
#include <unistd.h>
// #include <sstream>

#define BUTTON_PIN 38
const char *APP_NAME = "example_utilities";

// We initialise two lists of strings to use as app_data
const char *fruits[] = {"Peach", "Quince", "Date", "Tangerine",
                        "Pomelo", "Carambola", "Grape"};
const char *noble_gases[] = {"Helium", "Neon", "Argon", "Krypton",
                             "Xenon", "Radon", "Oganesson"};

double last_announce = 0.0;
bool send_announce = false;

class ExampleAnnounceHandler : public RNS::AnnounceHandler {
public:
  ExampleAnnounceHandler(const char *aspect_filter = nullptr)
      : AnnounceHandler(aspect_filter) {}
  virtual ~ExampleAnnounceHandler() {}
  virtual void received_announce(const RNS::Bytes &destination_hash,
                                 const RNS::Identity &announced_identity,
                                 const RNS::Bytes &app_data) {
    INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    INFO("ExampleAnnounceHandler: destination hash: " +
         destination_hash.toHex());
    if (announced_identity) {
      INFO("ExampleAnnounceHandler: announced identity hash: " +
           announced_identity.hash().toHex());
      INFO("ExampleAnnounceHandler: announced identity app data: " +
           announced_identity.app_data().toHex());
    }
    if (app_data) {
      INFO("ExampleAnnounceHandler: app data text: \"" + app_data.toString() +
           "\"");
    }
    INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         "!!!!!");
  }
};

void onPacket(const RNS::Bytes &data, const RNS::Packet &packet) {
  INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
       "!!!");
  INFO("onPacket: data: " + data.toHex());
  INFO("onPacket: text: \"" + data.toString() + "\"");
  // TRACE("onPacket: " + packet.debugString());
  INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

void onPingPacket(const RNS::Bytes &data, const RNS::Packet &packet) {
  INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  INFO("onPingPacket: data: " + data.toHex());
  INFO("onPingPacket: text: \"" + data.toString() + "\"");
  // TRACE("onPingPacket: " + packet.debugString());
  INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Interface tcp_interface(RNS::Type::NONE);
RNS::FileSystem universal_filesystem(RNS::Type::NONE);
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});

RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());

void reticulum_announce() {
  if (destination) {
    HEAD("Announcing destination...", RNS::LOG_TRACE);
    // destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum()
    // % 7]));
    //  test path
    // destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum()
    // % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
    //  test packet send
    destination.announce(
        RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
  }
}
void reticulum_setup() {
  INFO("Setting up Reticulum...");

  try {

    HEAD("Registering FileSystem with OS...", RNS::LOG_TRACE);
    universal_filesystem = new UniversalFileSystem();
    universal_filesystem.init();
    RNS::Utilities::OS::register_filesystem(universal_filesystem);

    HEAD("Registering TCPInterface instance with Transport...", RNS::LOG_TRACE);
    tcp_interface = new TCPInterface("TCPInterface_Main");
    tcp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
    RNS::Transport::register_interface(tcp_interface);
    tcp_interface.start();

    HEAD("Creating Reticulum instance...", RNS::LOG_TRACE);
    reticulum = RNS::Reticulum();
    reticulum.transport_enabled(true);
    reticulum.start();

    HEAD("Creating Identity instance...", RNS::LOG_TRACE);
    identity = RNS::Identity(false);
    RNS::Bytes prv_bytes;
#ifdef ARDUINO
    prv_bytes.assignHex(
        "78E7D93E28D55871608FF13329A226CABC3903A357388A035B360162FF6321570B092E0583772AB80BC425F99791DF5CA2CA0A985FF0415DAB419BBC64DDFAE8");
#else
    prv_bytes.assignHex(
        "E0D43398EDC974EBA9F4A83463691A08F4D306D4E56BA6B275B8690A2FBD9852E9EBE7C03BC45CAEC9EF8E78C830037210BFB9986F6CA2DEE2B5C28D7B4DE6B0");
#endif
    identity.load_private_key(prv_bytes);

    HEAD("Creating Destination instance...", RNS::LOG_TRACE);
    destination =
        RNS::Destination(identity, RNS::Type::Destination::IN,
                         RNS::Type::Destination::SINGLE, "app", "aspects");

    HEAD("Registering packet callback with Destination...", RNS::LOG_TRACE);
    destination.set_packet_callback(onPacket);
    destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

    {
      HEAD("Creating PING Destination instance...", RNS::LOG_TRACE);
      RNS::Destination ping_destination(identity, RNS::Type::Destination::IN,
                                        RNS::Type::Destination::SINGLE,
                                        "example_utilities", "echo.request");

      HEAD("Registering packet callback with PING Destination...",
           RNS::LOG_TRACE);
      ping_destination.set_packet_callback(onPingPacket);
      ping_destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
    }

    HEAD("Registering announce handler with Transport...", RNS::LOG_TRACE);
    RNS::Transport::register_announce_handler(announce_handler);

    HEAD("Ready!", RNS::LOG_TRACE);
  } catch (std::exception &e) {
    ERRORF("!!! Exception in reticulum_setup: %s", e.what());
  }
}
void reticulum_teardown() {
  INFO("Tearing down Reticulum...");

  RNS::Transport::persist_data();

  try {

    HEAD("Deregistering announce handler with Transport...", RNS::LOG_TRACE);
    RNS::Transport::deregister_announce_handler(announce_handler);

    HEAD("Deregistering Interface instances with Transport...", RNS::LOG_TRACE);
    RNS::Transport::deregister_interface(tcp_interface);

  } catch (std::exception &e) {
    ERRORF("!!! Exception in reticulum_teardown: %s", e.what());
  }
}

void setup() {
  RNS::loglevel(RNS::LOG_TRACE);
  reticulum_setup();
}

void loop() {
  reticulum.loop();
  tcp_interface.loop();

#ifdef ARDUINO
/*
        if ((RNS::Utilities::OS::time() - last_announce) > 10) {
                reticulum_announce();
                last_announce = RNS::Utilities::OS::time();
        }
*/
#endif
  if (send_announce) {
    reticulum_announce();
    send_announce = false;
  }
}

#ifndef ARDUINO
int getch() {
  termios oldt;
  termios newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  int ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

int main(void) {
  printf("Hello from Native on PlatformIO!\n");

  setup();

  bool run = true;
  while (run) {
    loop();
    int ch = getch();
    if (ch > 0) {
      switch (ch) {
      case 'a':
        reticulum_announce();
        break;
      case 'q':
        run = false;
        break;
      }
    }
    RNS::Utilities::OS::sleep(0.01);
  }

  reticulum_teardown();

  printf("Goodbye from Native on PlatformIO!\n");
}
#endif
