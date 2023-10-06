#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"

#ifndef NATIVE
#include <Arduino.h>
#endif
#include <stdlib.h>
#include <unistd.h>

#include <unity.h>

// Let's define an app name. We'll use this for all
// destinations we create. Since this basic example
// is part of a range of example utilities, we'll put
// them all within the app namespace "example_utilities"
const char* APP_NAME = "example_utilities";

// We initialise two lists of strings to use as app_data
const char* fruits[] = {"Peach", "Quince", "Date", "Tangerine", "Pomelo", "Carambola", "Grape"};
const char* noble_gases[] = {"Helium", "Neon", "Argon", "Krypton", "Xenon", "Radon", "Oganesson"};

void announceLoop(RNS::Destination* destination_1, RNS::Destination* destination_2) {
    // Let the user know that everything is ready
    RNS::log("Announce example running, hit enter to manually send an announce (Ctrl-C to quit)");

    // We enter a loop that runs until the users exits.
    // If the user hits enter, we will announce our server
    // destination on the network, which will let clients
    // know how to create messages directed towards it.
    while (true) {
      //zentered = input();
#ifndef NATIVE
      delay(1000);
#else
      usleep(1000);
#endif
      RNS::log("Sending announce...");
      
      // Randomly select a fruit
      //const char* fruit = fruits[rand() % sizeof(fruits)];
      const char* fruit = fruits[rand() % 7];
      RNS::log(fruit);
      //RNS::log(String("fruit: ") + fruit);

      // Send the announce including the app data
/*
      destination_1->announce(app_data=fruit.encode("utf-8"));
      RNS::log(
        "Sent announce from "+
        RNS.prettyhexrep(destination_1.hash)+
        " ("+destination_1.name+")"
      );
*/

      // Randomly select a noble gas
      //const char* noble_gas = noble_gases[rand() % sizeof(noble_gas)];
      const char* noble_gas = noble_gases[rand() % 7];
      RNS::log(noble_gas);
      //RNS::log(String("noble_gas: ") + noble_gas);

      // Send the announce including the app data
/*
      destination_2->announce(app_data=noble_gas.encode("utf-8"));
      RNS::log(
        "Sent announce from "+
        RNS.prettyhexrep(destination_2.hash)+
        " ("+destination_2.name+")"
      );
*/
    }
}

// This initialisation is executed when the program is started
void program_setup() {
    // We must first initialise Reticulum
    RNS::Reticulum* reticulum = new RNS::Reticulum();

    // Randomly create a new identity for our example
    RNS::Identity* identity = new RNS::Identity();

    // Using the identity we just created, we create two destinations
    // in the "example_utilities.announcesample" application space.
    //
    // Destinations are endpoints in Reticulum, that can be addressed
    // and communicated with. Destinations can also announce their
    // existence, which will let the network know they are reachable
    // and autoomatically create paths to them, from anywhere else
    // in the network.
    RNS::Destination* destination_1 = new RNS::Destination(
        identity,
        RNS::Destination::IN,
        RNS::Destination::SINGLE,
        APP_NAME,
        //z"announcesample",
        //z"fruits"
        nullptr
    );

    RNS::Destination* destination_2 = new RNS::Destination(
        identity,
        RNS::Destination::IN,
        RNS::Destination::SINGLE,
        APP_NAME,
        //z"announcesample",
        //z"noble_gases"
        nullptr
    );

    // We configure the destinations to automatically prove all
    // packets adressed to it. By doing this, RNS will automatically
    // generate a proof for each incoming packet and transmit it
    // back to the sender of that packet. This will let anyone that
    // tries to communicate with the destination know whether their
    // communication was received correctly.
    //zdestination_1->set_proof_strategy(RNS::Destination::PROVE_ALL);
    //zdestination_2->set_proof_strategy(RNS::Destination::PROVE_ALL);

    // We create an announce handler and configure it to only ask for
    // announces from "example_utilities.announcesample.fruits".
    // Try changing the filter and see what happens.
    //zannounce_handler = ExampleAnnounceHandler(
    //z    aspect_filter="example_utilities.announcesample.fruits";
    //z)

    // We register the announce handler with Reticulum
    //zRNS::Transport.register_announce_handler(announce_handler);
    
    // Everything's ready!
    // Let's hand over control to the announce loop
    announceLoop(destination_1, destination_2);
}


#ifndef NATIVE

void setup() {

  Serial.begin(115200);
  RNS::log("Hello T-Beam from PlatformIO!");

  program_setup();
}

void loop() {
}

#else

int main(int argc, char **argv) {
  RNS::log("Hello Native from PlatformIO!");
  UNITY_BEGIN();
  RUN_TEST(program_setup);
  UNITY_END();
}

#endif
