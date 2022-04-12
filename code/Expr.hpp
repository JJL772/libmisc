/**
----------------------------------------------------------------------
Expr.hpp - A simple logical expression evaluator
----------------------------------------------------------------------

----------------------------------------------------------------------
-                      LICENSE FOR THIS COMPONENT                    -
----------------------------------------------------------------------
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
----------------------------------------------------------------------
*/
#pragma once

#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <cstring>
#include <cassert>

namespace expr
{

enum class Error {
	OK = 0,
	BadToken,
	UnmatchedClosingParenths,
	BufTooSmall,
	StackUnderflow,
	TooManyOps,
	BadOp,
	UndefinedVar
};

template <std::size_t EXPR_SIZE = 256>
class BoolExpression {
public:
	static constexpr int PARAM_MAX = 'Z' - 'A' + 1;
	std::string m_vars[PARAM_MAX];
	bool m_varVals[PARAM_MAX];

	char m_expr[EXPR_SIZE]; // Initial expression
	char m_rpn[EXPR_SIZE];	// Resultant RPN expression

public:
	BoolExpression(const char *expression);
	BoolExpression(const std::string &expr);

#if __cplusplus > 201703l
	BoolExpression(std::string_view expr);
#endif // C++17

	enum Operator {
		OR = '|',
		AND = '&',
		COM = '~',
		NOT = '!',
		XOR = '^'
	};

	/**
	 * @brief Defines a variable's value
	 * @param varName Name of the variable
	 * @param value Value
	 * @return int Index of the new or existing variable with that name. -1 is returned if max number of vars has been
	 * reached
	 */
	int define(const char *varName, bool value);

	/**
	 * @brief Defines a variable's value
	 * @param varIndex Index of the variable.
	 * @param value Value
	 */
	void set(int varIndex, bool value);

	/**
	 * @brief Parses the logical expression
	 * @return Error
	 */
	Error parse();

	Error eval(bool &result);

private:
	inline int precedence(char c) const {
		return (((c) == OR || (c) == XOR) ? 0 : (c) == AND ? 1 : 2);
	};
	inline bool isOp(char c) const {
		return (c == OR || c == AND || c == COM || c == NOT || c == XOR);
	};
	inline bool isVar(char x) const {
		return (x <= 'Z' && x >= 'A');
	}
	inline bool isText(char x) const {
		return ((x >= '0' && x <= '9') || (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || x == '_');
	}
	inline int varIndex(const char *s) const {
		int i = 0;
		for (const auto &v : m_vars) {
			if (!std::strcmp(v.c_str(), s))
				return i;
			++i;
		}
		return -1;
	}
};

template <std::size_t EXPR_SIZE>
inline BoolExpression<EXPR_SIZE>::BoolExpression(const char *expression) {
	std::strncpy(m_expr, expression, EXPR_SIZE);
	m_expr[EXPR_SIZE - 1] = 0;
}

template <std::size_t EXPR_SIZE>
inline BoolExpression<EXPR_SIZE>::BoolExpression(const std::string &expr) {
	std::strncpy(m_expr, expr.c_str(), EXPR_SIZE);
	m_expr[EXPR_SIZE - 1] = 0;
}

#if __cplusplus > 201703l
template <std::size_t EXPR_SIZE>
inline BoolExpression<EXPR_SIZE>::BoolExpression(std::string_view expr) {
	std::strncpy(m_expr, expr.c_str(), EXPR_SIZE);
	m_expr[EXPR_SIZE - 1] = 0;
}
#endif // C++17

template <std::size_t EXPR_SIZE>
inline int BoolExpression<EXPR_SIZE>::define(const char *varName, bool value) {
	for (int i = 0; i < PARAM_MAX; i++) {
		if (m_vars[i].empty()) {
			m_vars[i] = varName;
			m_varVals[i] = value;
			return i;
		}
	}
	return -1;
}

template <std::size_t EXPR_SIZE>
inline void BoolExpression<EXPR_SIZE>::set(int varIndex, bool value) {
	assert(varIndex >= 0 && varIndex <= PARAM_MAX);
	m_varVals[varIndex] = value;
}

/* Takes an expression in infix notation and converts it to RPN using the shunting yard algorithm */
template <std::size_t EXPR_SIZE>
Error BoolExpression<EXPR_SIZE>::parse() {
	std::size_t rpn_size = sizeof(m_rpn);
	char *rpn = m_rpn;
	const char *expr = m_expr;

	char out_queue[256]; /* Output queue */
	char opstack[256];
	std::memset(out_queue, 0, sizeof(out_queue));

	int queuei = 0;
	int stacki = 0;

	const char *s = expr;
	for (s = expr; s && *s; s++) {
		if (isspace(*s))
			continue;
		if (*s == '(') {
			opstack[stacki++] = *s;
			continue;
		}
		if (isOp(*s)) {
			while (opstack[stacki - 1] != '(' && stacki > 0 && precedence(opstack[stacki - 1]) > precedence(*s)) {
				out_queue[queuei++] = opstack[stacki - 1];
				stacki--;
			}
			opstack[stacki++] = *s;
			continue;
		}
		if (*s == ')') {
			while (opstack[stacki - 1] != '(' && stacki > 0) {
				out_queue[queuei++] = opstack[stacki - 1];
				stacki--;
			}
			if (stacki <= 0 || opstack[stacki - 1] != '(')
				return Error::UnmatchedClosingParenths;
			opstack[stacki - 1] = 0;
			stacki--;
			continue;
		}
		/* Consume a token comprised of a-Z, _ and 0-9 and then obtain the ID for it */
		const char *z;
		char buf[256];
		char *b = buf;
		*b = 0;
		for (z = s; z && isText(*z); z++)
			*b++ = *z;
		*b = 0; // Terminate
		if (z != s) {
			auto idx = varIndex(buf);
			if (idx == -1) {
				return Error::UndefinedVar;
			}
			out_queue[queuei++] = idx + 'A';
			continue;
		}

		//	if (IS_VAR(*s)) {
		//		out_queue[queuei++] = *s;
		//		continue;
		//	}
		return Error::BadToken;
	}
	while (stacki > 0) {
		out_queue[queuei++] = opstack[stacki - 1];
		stacki--;
	}

	if (queuei >= rpn_size)
		return Error::BufTooSmall;
	std::strncpy(rpn, out_queue, queuei);
	rpn[queuei] = 0;

	return Error::OK;
}

/* Evaluates the boolean expression in RPN */
template <std::size_t EXPR_SIZE>
Error BoolExpression<EXPR_SIZE>::eval(bool &result) {
	const char *rpn = m_rpn;
	bool varstack[256];
	int vstacki = 0;

	auto opCheck = [](int vstacki, int n) -> bool {
		if (vstacki < n) {
			return false;
		}
		return true;
	};

	const char *s;
	for (s = rpn; s && *s; ++s) {
		if (isVar(*s)) {
			varstack[vstacki++] = m_varVals[*s - 'A']; // varvals[*s - 'A'];
			continue;
		}
		else if (isOp(*s)) {
			bool a, b;
			switch (*s) {
			case OR:
				if (!opCheck(vstacki, 2)) {
					return Error::TooManyOps;
				}
				a = varstack[vstacki - 1];
				b = varstack[vstacki - 2];
				vstacki--;
				varstack[vstacki] = a || b;
				break;
			case AND:
				if (!opCheck(vstacki, 2)) {
					return Error::TooManyOps;
				}
				a = varstack[vstacki - 1];
				b = varstack[vstacki - 2];
				vstacki--;
				varstack[vstacki] = a && b;
				break;
			case COM:
			case NOT:
				if (!opCheck(vstacki, 1)) {
					return Error::TooManyOps;
				}
				//vstacki -= 1;
				varstack[vstacki-1] = !varstack[vstacki-1];
				break;
			case XOR:
				if (!opCheck(vstacki, 2)) {
					return Error::TooManyOps;
				}
				a = varstack[vstacki - 1];
				b = varstack[vstacki - 2];
				vstacki--;
				varstack[vstacki] = a ^ b;
				break;
			default:
				return Error::BadOp;
			}
		}
	}

	result = varstack[vstacki];
	return Error::OK;
}

} // namespace expr