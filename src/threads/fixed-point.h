#ifndef THREADS_FIXED_POINT_H
#define THREADs_FIXED_POINT_H

#include <stdint.h>

#define FP_P 17

#define FP_Q 14

#define FP_F (1 << FP_Q)

#define int_to_fixed_point(N) (N * FP_F)

#define fixed_point_to_int_round_to_zero(X) (X / FP_F)

#define fixed_point_to_int_round_nearest(X)               \
        (X >= 0 ? ((X + (FP_F / 2)) / FP_F) : ((X - (FP_F / 2)) / FP_F))

#define fixed_point_add_fixed_point(X, Y) (X + Y)

#define fixed_point_subtract_fixed_point(X, Y) (X - Y)

#define fixed_point_add_int(X, N) (X + (N * FP_F))

#define fixed_point_subtract_int(X, N) (X - (N * FP_F))

#define fixed_point_multiply_fixed_point(X, Y) \
        ((((int64_t) X) * Y) / FP_F)

#define fixed_point_multiply_int(X, N) (X * N)

#define fixed_point_divide_fixed_point(X, Y)  \
        ((((int64_t) X) * FP_F) / Y)

#define fixed_point_divide_int(X, N) (X / N)

#endif /* threads/fixed-point.h */
