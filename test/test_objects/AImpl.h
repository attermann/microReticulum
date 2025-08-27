#pragma once

#include "B.h"

class AImpl {
public:
	using Ptr = std::shared_ptr<AImpl>;
	AImpl() {}
	AImpl(const B& b) : _b(b) {}
	B getB();
protected:
	B _b;
};
