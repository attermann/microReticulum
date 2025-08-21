#pragma once

#include "A.h"

class BData {
protected:
	BData() {}
	BData(const A& a) : _a(a) {}
protected:
	A _a;
friend class B;
};
