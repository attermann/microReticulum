// Only include if building locally and NOT testing
#if defined(LIBRARY_TEST) && !defined(PIO_UNIT_TESTING)
#include <stdio.h>

#ifdef ARDUINO

void setup() {
}
void loop() {
}

#else

int main(void) {
	printf("\nSilly rabbit, microReticulum is a library!\n\nSee the examples directory for example programs that make use of this library.\n\n");
}

#endif

#endif
