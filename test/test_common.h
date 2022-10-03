
#include <stdio.h>
#include <stdlib.h>

#define T_INFO(...) {printf(__VA_ARGS__);fflush(stdout);}
#define T_WARN(...) {fprintf(stderr, __VA_ARGS__);}
#define T_TESTCASE() {printf("====== %s ======\n", __func__);}
