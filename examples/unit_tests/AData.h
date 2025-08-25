#pragma once

#include "B.h"

class AData {
protected:
	AData() {}
	AData(const B& b) : _b(b) {}
protected:
	B _b;
friend class A;
};
