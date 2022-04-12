
#include "time.h"
#include "assert.h"


#define KEYVALUES_IMPLEMENTATION
#include "../code/KeyValues.hpp"

static void kv_perftest1();

int main() {
	//auto* kv = KeyValues::FromFile("test.kv");
	//kv->DumpToStream(stdout);
	
	kv_perftest1();
}

class Timer {
	unsigned long long start_, end_;
public:
	void start() {
		timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		start_ = tp.tv_sec * 1e3 + tp.tv_nsec / 1e6;
	}
	
	void end() {
		timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		end_ = tp.tv_sec * 1e3 + tp.tv_nsec / 1e6;
	}
	
	void dump(FILE* fp = stdout) {
		fprintf(fp, "%lf ms (%lf s) elapsed\n", (double)(end_-start_), ((double)end_-start_)/1e3);
	}
};

static void kv_perftest1() {
	
	Timer timer;
	
	printf("kv_perftest1: ");
	timer.start();
	auto* kv = KeyValues::FromFile("large_test.kv");
	assert(kv);
	timer.end();
	timer.dump();
}