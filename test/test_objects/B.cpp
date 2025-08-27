#include "B.h"

#include "A.h"
#include <stdio.h>

#if TEST_OBJECT_DATA

#include "BData.h"

B::B() {
	printf("B() this=0x%x B.Id=%x\n", this, getId());
}

B::B(const B& ref) : _data(ref._data) {
	printf("B(B) this=0x%x B.Id=%x\n", this, getId());
}

B::B(const A& a) : _data(new BData(a)) {
	printf("B(A) this=0x%x B.Id=%x A.Id=%x\n", this, getId(), _data->_a.getId());
}

B& B::operator = (const B& ref) {
	_data = ref._data;
	printf("=(B) this=0x%x B.Id=%x\n", this, getId());
	return *this;
}

A B::getA() {
	printf("getA() this=0x%x B.Id=%x A.Id=%x\n", this, getId(), _data->_a.getId());
	return _data->_a;
}

long B::getId() {
	return (long)_data.get();
}

#elif TEST_OBJECT_IMPL

#include "BImpl.h"

B::B() {
	printf("B() this=0x%x B.Id=%x\n", this, getId());
}

B::B(const B& ref) : _impl(ref._impl) {
	printf("B(B) this=0x%x B.Id=%x\n", this, getId());
}

B::B(const A& a) : _impl(new BImpl(a)) {
	//printf("B(A) B.Id=%x A.Id=%x\n", getId(), _impl->_a.getId());
	printf("B(A) this=0x%x B.Id=%x\n", this, getId());
}

B& B::operator = (const B& ref) {
	_impl = ref._impl;
	printf("=(B) this=0x%x B.Id=%x\n", this, getId());
	return *this;
}

A B::getA() {
	//printf("getA() B.Id=%x A.Id=%x\n", getId(), _impl->_a.getId());
	printf("getA() this=0x%x B.Id=%x\n", this, getId());
	return _impl->getA();
}

long B::getId() {
	return (long)_impl.get();
}

#else

B::B() {
}

B::B(const A& a) {
	_a = a;
}

A B::getA() {
	return _a;
}

long B::getId() {
	return 0;
}

#endif
