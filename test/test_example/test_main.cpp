#include <unity.h>

void setUp(void) {
    // set stuff up here before each test
}

void tearDown(void) {
    // clean stuff up here after each test
}

void testExample() {
	bool success = true;
    TEST_ASSERT_EQUAL(true, success);
}


int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(testExample);
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
