#include <unity.h>

#include "A.h"
#include "B.h"

#include <stdio.h>

void testObjects() {

	printf("Creating a...\n");
	A a = A();
	
	printf("Creating b...\n");
	B b = B();

	// Following assignments are invalid because a and b are empty
	//printf("Assigning b1...\n");
	//B b1 = a.getB();
	//printf("Assigning a1...\n");
	//A a1 = b.getA();

	printf("Creating ab...\n");
	A ab = A(b);
	
	printf("Creating ba...\n");
	B ba = B(a);

	printf("Assigning b2...\n");
	B b2 = ab.getB();
	
	printf("Assigning a2...\n");
	A a2 = ba.getA();

}


void setUp(void) {
	// set stuff up here before each test
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testObjects);
	return UNITY_END();
}

// For native dev-platform or for some embedded frameworks
int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
// For Arduino framework
void setup() {
	// Wait ~2 seconds before the Unity test runner
	// establishes connection with a board Serial interface
	delay(2000);
	
	runUnityTests();
}
void loop() {}
#endif

// For ESP-IDF framework
void app_main() {
	runUnityTests();
}
