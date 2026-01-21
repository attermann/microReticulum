// Only include if building locally and NOT testing
#if defined(LIBRARY_TEST) && !defined(PIO_UNIT_TESTING)
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
	Serial.begin(115200);
	Serial.print("\nSilly rabbit, microReticulum is a library!\n\nSee the examples directory for example programs that make use of this library.\n\n");
}
void loop() {
}

#else

int main(void) {
	printf("\nSilly rabbit, microReticulum is a library!\n\nSee the examples directory for example programs that make use of this library.\n\n");
}

#endif

#endif
