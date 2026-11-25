// ring_int.h: the coefficient ring, int64 with every operation checked. A
// port of integer_math.py's IntegerMath (zero, one, add, mul, negate,
// power), where Python's integers never overflow: here an overflow stops
// the program, naming the operation and its operands on stderr, never a
// wrap.

#ifndef COOK_LEVIN_RING_INT_H
#define COOK_LEVIN_RING_INT_H

#include <stdint.h>

#define RING_ZERO ((int64_t)0)
#define RING_ONE ((int64_t)1)

int64_t ring_add(int64_t a, int64_t b);
int64_t ring_mul(int64_t a, int64_t b);
int64_t ring_negate(int64_t n);
// n ** exp, exp >= 0 (a negative exp stops, as Python's would leave the
// integers); 0 ** 0 is 1, as Python's.
int64_t ring_power(int64_t n, int64_t exp);

#endif
