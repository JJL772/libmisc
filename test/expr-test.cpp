
#include "../code/Expr.hpp"

#include <stdlib.h>

using namespace expr;

static void test1();
static void test2();
static void test3();

int main(int argc, char**argv) {

	test1();
	test2();
	test3();

	return 0;
}

static void test1() {
	{
		auto expr = BoolExpression<256>("A&B");
		expr.define("A", true);
		expr.define("B", false);

		assert(expr.parse() == expr::Error::OK);

		bool val = false;
		assert(expr.eval(val) == expr::Error::OK);

		printf("Result was %s (expected false)\n", val ? "true" : "false");
		assert(val == false);
	}

	{
		auto expr = BoolExpression<256>("A&B");
		expr.define("A", true);
		expr.define("B", true);

		assert(expr.parse() == expr::Error::OK);

		bool val = false;
		assert(expr.eval(val) == expr::Error::OK);

		printf("Result was %s (expected true)\n", val ? "true" : "false");
		assert(val == true);
	}
}

static void test2() {

	{
		auto expr = BoolExpression<256>("A&!B");
		expr.define("A", true);
		expr.define("B", false);

		assert(expr.parse() == expr::Error::OK);

		bool val = false;
		assert(expr.eval(val) == expr::Error::OK);

		printf("Result was %s (expected true)\n", val ? "true" : "false");
		assert(val == true);
	}

	{
		auto expr = BoolExpression<256>("A|!B");
		expr.define("A", false);
		expr.define("B", true);

		assert(expr.parse() == expr::Error::OK);

		bool val = false;
		assert(expr.eval(val) == expr::Error::OK);

		printf("Result was %s (expected false)\n", val ? "true" : "false");
		assert(val == false);
	}
}

static void test3() {

	{
		auto expr = BoolExpression<256>("!A&B|(C&D)");
		expr.define("A", true);
		expr.define("B", false);
		expr.define("C", true);
		expr.define("D", true);

		assert(expr.parse() == expr::Error::OK);

		bool val = false;
		assert(expr.eval(val) == expr::Error::OK);

		printf("Result was %s (expected true)\n", val ? "true" : "false");
		assert(val == true);
	}

	{
		auto expr = BoolExpression<256>("A&(~B|C)");
		expr.define("A", true);
		expr.define("B", true);
		expr.define("C", false);

		assert(expr.parse() == expr::Error::OK);

		bool val = false;
		assert(expr.eval(val) == expr::Error::OK);

		printf("Result was %s (expected false)\n", val ? "true" : "false");
		assert(val == false);
	}
}


