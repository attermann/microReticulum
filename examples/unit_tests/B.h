#pragma once

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
	A getA();
	long getId();
protected:
	A _a;
};

#endif
