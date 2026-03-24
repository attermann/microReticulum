/*
 * Copyright (c) 2023 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

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
