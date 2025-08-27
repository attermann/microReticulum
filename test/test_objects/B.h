#pragma once

//#define TEST_OBJECT_DATA 1
#define TEST_OBJECT_IMPL 1
#if TEST_OBJECT_DATA

#include <memory>

class BData;
class A;

class B {
public:
	B();
	B(const A& a);
	B(const B& ref);
	B& operator = (const B& ref);
	operator bool() const {
		return _data.get() != nullptr;
	}
	A getA();
	long getId();
protected:
	std::shared_ptr<BData> _data;
};

#elif TEST_OBJECT_IMPL

#include <memory>

class BImpl;
class A;

class B {
public:
	B();
	B(const A& a);
	B(const B& ref);
	B& operator = (const B& ref);
	operator bool() const {
		return _impl.get() != nullptr;
	}
	A getA();
	long getId();
protected:
	std::shared_ptr<BImpl> _impl;
};

#else

#include "A.h"

class B {
public:
	B();
	B(const A& a);
	operator bool() const {
		return true;
	}
	A getA();
	long getId();
protected:
	A _a;
};

#endif
