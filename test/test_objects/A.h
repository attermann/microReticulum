#pragma once

//#define TEST_OBJECT_DATA 1
#define TEST_OBJECT_IMPL 1
#if TEST_OBJECT_DATA

#include <memory>

class AData;
class B;

class A {
public:
	A();
	A(const B& b);
	A(const A& ref);
	A& operator = (const A& ref);
	operator bool() const {
		return _data.get() != nullptr;
	}
	B getB();
	long getId();
protected:
	std::shared_ptr<AData> _data;
};

#elif TEST_OBJECT_IMPL

#include <memory>

class AImpl;
class B;

class A {
public:
	A();
	A(const B& b);
	A(const A& ref);
	A& operator = (const A& ref);
	operator bool() const {
		return _impl.get() != nullptr;
	}
	B getB();
	long getId();
protected:
	std::shared_ptr<AImpl> _impl;
};

#else

#include "B.h"

class A {
public:
	A();
	A(const B& b);
	operator bool() const {
		return true;
	}
	B getB();
	long getId();
protected:
	B _b;
};

#endif
