#include "A.h"

#include "B.h"
#include <stdio.h>

#if TEST_OBJECT_DATA

#include "AData.h"

A::A() {
	printf("A() this=0x%x A.Id=%x\n", this, getId());
}

A::A(const A& ref) : _data(ref._data) {
	printf("A(A) this=0x%x A.Id=%x\n", this, getId());
}

A::A(const B& b) : _data(new AData(b)) {
	printf("A(B) this=0x%x A.Id=%x B.Id=%x\n", this, getId(), _data->_b.getId());
}

A& A::operator = (const A& ref) {
	_data = ref._data;
	printf("=(A) this=0x%x A.Id=%x\n", this, getId());
	return *this;
}

B A::getB() {
	printf("getB() this=0x%x A.Id=%x B.Id=%x\n", this, getId(), _data->_b.getId());
	return _data->_b;
}

long A::getId() {
	return (long)_data.get();
}

#elif TEST_OBJECT_IMPL

#include "AImpl.h"

A::A() {
//A::A(AImpl::Ptr impl) : _impl(impl) {
	printf("A() this=0x%x A.Id=%x\n", this, getId());
}

A::A(const A& ref) : _impl(ref._impl) {
	printf("A(A) this=0x%x A.Id=%x\n", this, getId());
}

A::A(const B& b) : _impl(new AImpl(b)) {
	//printf("A(B) A.Id=%x B.Id=%x\n", getId(), _impl->_b.getId());
	printf("A(B) this=0x%x A.Id=%x\n", this, getId());
}

A& A::operator = (const A& ref) {
	_impl = ref._impl;
	printf("=(A) this=0x%x A.Id=%x\n", this, getId());
	return *this;
}

B A::getB() {
	//printf("getB() A.Id=%x B.Id=%x\n", getId(), _impl->_b.getId());
	printf("getB() this=0x%x A.Id=%x\n", this, getId());
	return _impl->getB();
}

long A::getId() {
	return (long)_impl.get();
}

#else

A::A() {
}

A::A(const B& b) {
	_b = b;
}

B A::getB() {
	return _b;
}

long A::getId() {
	return 0;
}

#endif
