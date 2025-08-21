
#include "A.h"
#include "B.h"

#include <stdio.h>

void testObjects() {

	printf("Creating a...\n");
	A a = A();
	printf("Creating b...\n");
	B b = B();

	// Following assignments are invalid because a and b are empty
	//printf("Assigning b1...\n");
	//B b1 = a.getB();
	//printf("Assigning a1...\n");
	//A a1 = b.getA();

	printf("Creating ab...\n");
	A ab = A(b);
	printf("Creating ba...\n");
	B ba = B(a);

	printf("Assigning b2...\n");
	B b2 = ab.getB();
	printf("Assigning a2...\n");
	A a2 = ba.getA();

}
