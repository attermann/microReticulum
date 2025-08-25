/*
##########################################################
# This RNS example demonstrates how to set up a link to  #
# a destination, and pass data back and forth over it.   #
##########################################################
*/

#include <UDPInterface.h>
#include <UniversalFileSystem.h>

#include <Reticulum.h>
#include <Interface.h>
#include <Link.h>
#include <Identity.h>
#include <Destination.h>
#include <Packet.h>
#include <Transport.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <SPIFFS.h>
#else
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

// Let's define an app name. We'll use this for all
// destinations we create. Since this echo example
// is part of a range of example utilities, we'll put
// them all within the app namespace "example_utilities"
#define APP_NAME "example_utilities"

RNS::Reticulum reticulum;
RNS::FileSystem universal_filesystem(RNS::Type::NONE);
RNS::Interface udp_interface(RNS::Type::NONE);


/*
##########################################################
#### Server Part #########################################
##########################################################
*/

// A reference to the latest client link that connected
RNS::Link latest_client_link({RNS::Type::NONE});

void client_disconnected(RNS::Link& link) {
	RNS::log("Client disconnected");
}

void server_packet_received(const RNS::Bytes& message, const RNS::Packet& packet) {

	// When data is received over any active link,
	// it will all be directed to the last client
	// that connected.
	std::string text = message.toString();
	RNS::log("Received data on the link: "+text);

	std::string reply_text = "I received \""+text+"\" over the link";
	RNS::Bytes reply_data(reply_text);
	// CBA TODO: Add Packet constructor that accepts Link but doesn't require Destination
	//RNS::Packet(RNS::Type::NONE, latest_client_link, reply_data).send();
	RNS::Packet(latest_client_link, reply_data).send();
}

// When a client establishes a link to our server
// destination, this function will be called with
// a reference to the link.
void client_connected(RNS::Link& link) {
	RNS::log("Client connected");
	link.set_link_closed_callback(client_disconnected);
	link.set_packet_callback(server_packet_received);
	latest_client_link = link;
}

void server_loop(RNS::Destination& destination) {
	// Let the user know that everything is ready
	RNS::log(
		"Link example "+
		destination.hash().toHex()+
		" running, waiting for a connection."
	);

	RNS::log("Hit enter to manually send an announce (Ctrl-C to quit)");

	// We enter a loop that runs until the users exits.
	// If the user hits enter, we will announce our server
	// destination on the network, which will let clients
	// know how to create messages directed towards it.
	while (true) {
		reticulum.loop();
		udp_interface.loop();
		// Non-blocking input
		char ch;
		while (read(STDIN_FILENO, &ch, 1) > 0) {
			if (ch == '\n') {
				destination.announce();
				RNS::log("Sent announce from "+destination.hash().toHex());
			}
		}
	}
}

// This initialisation is executed when the users chooses
// to run as a server
void server() {

	// Randomly create a new identity for our link example
	RNS::Identity server_identity = RNS::Identity();

	// We create a destination that clients can connect to. We
	// want clients to create links to this destination, so we
	// need to create a "single" destination type.
	RNS::Destination server_destination = RNS::Destination(
		server_identity,
		RNS::Type::Destination::IN,
		RNS::Type::Destination::SINGLE,
		APP_NAME,
		"linkexample"
	);

	// We configure a function that will get called every time
	// a new client creates a link to this destination.
	server_destination.set_link_established_callback(client_connected);

	// Everything's ready!
	// Let's Wait for client requests or user input
	server_loop(server_destination);
}


/*
##########################################################
#### Client Part #########################################
##########################################################
*/

// A reference to the server link
RNS::Link server_link({RNS::Type::NONE});

// This function is called when a link
// has been established with the server
void link_established(RNS::Link& link) {
    // We store a reference to the link
    // instance for later use
    server_link = link;

    // Inform the user that the server is
    // connected
    RNS::log("Link established with server, enter some text to send, or \"quit\" to quit");
}

// When a link is closed, we'll inform the
// user, and exit the program
void link_closed(RNS::Link& link) {
	if (link.teardown_reason() == RNS::Type::Link::TIMEOUT) {
		RNS::log("The link timed out, exiting now");
	}
	else if (link.teardown_reason() == RNS::Type::Link::DESTINATION_CLOSED) {
		RNS::log("The link was closed by the server, exiting now");
	}
	else {
		RNS::log("Link closed, exiting now");
	}

	//RNS::Reticulum::exit_handler();
	//RNS::Utilities::OS::sleep(1.5);
	_exit(0);
}

// When a packet is received over the link, we
// simply print out the data.
void client_packet_received(const RNS::Bytes& message, const RNS::Packet& packet) {
	std::string text = message.toString();
    RNS::log("Received data on the link: "+text);
    printf("> ");
	fflush(stdout);
}

void client_loop() {
	// Wait for the link to become active
    RNS::log("Waiting for link to become active...");
    while (!server_link) {
		reticulum.loop();
		udp_interface.loop();
		RNS::Utilities::OS::sleep(0.1);
	}

	std::string text;
	printf("> ");
	fflush(STDIN_FILENO);
	bool should_quit = false;
    while (!should_quit) {
		reticulum.loop();
		udp_interface.loop();

		// Non-blocking input
		char ch;
		while (read(STDIN_FILENO, &ch, 1) > 0) {
			if (ch == '\n') {

				// Check if we should quit the example
				if (text == "quit" || text == "q" || text == "exit") {
					should_quit = true;
					server_link.teardown();
				}

				// If not, send the entered text over the link
				if (text != "") {
					RNS::Bytes data(text);
					if (data.size() <= RNS::Type::Link::MDU) {
printf("(sending data: %s)\n", text.c_str());
						// CBA TODO: Add Packet constructor that accepts Link but doesn't require Destination
						//RNS::Packet(RNS::Type::NONE, server_link, data).send();
						RNS::Packet(server_link, data).send();
					}
					else {
						RNS::log(
							"Cannot send this packet, the data size of "+
							std::to_string(data.size())+" bytes exceeds the link packet MDU of "+
							std::to_string(RNS::Type::Link::MDU)+" bytes",
							RNS::LOG_ERROR
						);
					}
				}

				text.clear();

				printf("> ");
				fflush(STDIN_FILENO);
			} else {
				text += ch;
			}
		}

		RNS::Utilities::OS::sleep(0.1);
	}
}

// This initialisation is executed when the users chooses
// to run as a client
void client(const char* destination_hexhash) {
	// We need a binary representation of the destination
	// hash that was entered on the command line
	RNS::Bytes destination_hash;
	try {
		int dest_len = (RNS::Type::Reticulum::TRUNCATED_HASHLENGTH/8)*2;
		if (strlen(destination_hexhash) != dest_len) {
			throw std::invalid_argument("Destination length is invalid, must be "+std::to_string(dest_len)+" hexadecimal characters ("+std::to_string(dest_len/2)+" bytes).");
		}

		destination_hash.assignHex(destination_hexhash);
	}
	catch (std::exception& e) {
		RNS::log("Invalid destination entered. Check your input!", RNS::LOG_ERROR);
		return;
	}

    // Check if we know a path to the destination
    if (!RNS::Transport::has_path(destination_hash)) {
        RNS::log("Destination is not yet known. Requesting path and waiting for announce to arrive...");
        RNS::Transport::request_path(destination_hash);
        while (!RNS::Transport::has_path(destination_hash)) {
			reticulum.loop();
			udp_interface.loop();
			RNS::Utilities::OS::sleep(0.1);
		}
	}

    // Recall the server identity
    RNS::Identity server_identity = RNS::Identity::recall(destination_hash);

    // Inform the user that we'll begin connecting
    RNS::log("Establishing link with server...");

    // When the server identity is known, we set
    // up a destination
    RNS::Destination server_destination = RNS::Destination(
        server_identity,
		RNS::Type::Destination::OUT,
		RNS::Type::Destination::SINGLE,
        APP_NAME,
        "linkexample"
    );

    // And create a link
    RNS::Link link = RNS::Link(server_destination);

    // We set a callback that will get executed
    // every time a packet is received over the
    // link
    link.set_packet_callback(client_packet_received);

    // We'll also set up functions to inform the
    // user when the link is established or closed
    link.set_link_established_callback(link_established);
    link.set_link_closed_callback(link_closed);

    // Everything is set up, so let's enter a loop
    // for the user to interact with the example
    client_loop();
}


/*
##########################################################
#### Program Startup #####################################
##########################################################
*/

// Signal handler function
void cleanup_handler(int signum) {
	if (signum == SIGINT) {
		printf("\nCtrl+C detected. Performing cleanup...\n");

		printf("Cleanup complete. Exiting.\n");
		_exit(0);
	}
}

// This part of the program runs at startup,
// and parses input of from the user, and then
// starts up the desired program mode.
int main(int argc, char *argv[]) {

#if defined(MEM_LOG)
	RNS::loglevel(RNS::LOG_MEM);
#else
	RNS::loglevel(RNS::LOG_NOTICE);
	//RNS::loglevel(RNS::LOG_TRACE);
#endif

	// Register the signal handler for SIGINT
    signal(SIGINT, cleanup_handler);

	// Setup non-blocking input
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	// Initialize and register filesystem
	universal_filesystem = new UniversalFileSystem();
	universal_filesystem.init();
	RNS::Utilities::OS::register_filesystem(universal_filesystem);

	// Initialize and register interface
	udp_interface = new UDPInterface();
	udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
	RNS::Transport::register_interface(udp_interface);
	udp_interface.start();

	// Initialize and start Reticulum
	reticulum.start();

	if (argc <= 1) {
		printf("\nMust specify a destination for client mode, or \"-s\" or \"--server\" for server mode.\n\n");
		return -1;
	}
	if (strcmp(argv[1], "--server") == 0 || strcmp(argv[1], "-s") == 0) {
		server();
	}
	else {
		client(argv[1]);
	}

	printf("\nLoop exited. Performing cleanup...\n");

	printf("Cleanup complete. Exiting.\n");

	return 0;
}
