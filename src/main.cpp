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
