//#include <unity.h>

#include "Reticulum.h"
#include "Bytes.h"

#include <assert.h>

void testReference() {

	RNS::Reticulum reticulum_default;
	assert(reticulum_default);

	RNS::Reticulum reticulum_none({RNS::Type::NONE});
	assert(!reticulum_none);

	RNS::Reticulum reticulum_default_copy(reticulum_default);
	assert(reticulum_default_copy);

	RNS::Reticulum reticulum_none_copy(reticulum_none);
	assert(!reticulum_none_copy);

}

/*
int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(testReference);
	return UNITY_END();
}
*/
