#pragma once

#include "A.h"

class BImpl {
public:
	using Ptr = std::shared_ptr<BImpl>;
	BImpl() {}
	BImpl(const A& a) : _a(a) {}
	A getA();
protected:
	A _a;
};
