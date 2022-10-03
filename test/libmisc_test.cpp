
#include "test_common.h"

#include "../include/libmisc/timer.hpp"

#include "time.h"
#include "assert.h"


#define KEYVALUES_IMPLEMENTATION
#include "../code/KeyValues.hpp"

static void kv_perftest1();

int main() {
	kv_perftest1();
}

static void kv_perftest1() {
	T_TESTCASE();

	libmisc::timer timer;
	
	timer.begin();
	auto* kv = KeyValues::FromFile("large_test.kv");
	assert(kv);
	timer.end();
	timer.display();
}
