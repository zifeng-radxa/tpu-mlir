#ifndef OP_CODE_H_
#define OP_CODE_H_

#define BM_REDUCE_MEAN 0
#define BM_REDUCE_SUM  1
#define BM_REDUCE_MAX  2
#define BM_REDUCE_MIN  3
#define BM_REDUCE_PROD 4
#define BM_REDUCE_ALL  5
#define BM_REDUCE_ANY  6
#define BM_REDUCE_L2   7

#define BM_BINARY_ADD 0
#define BM_BINARY_SUB 1
#define BM_BINARY_MUL 2
#define BM_BINARY_DIV 3
#define BM_BINARY_MAX 4
#define BM_BINARY_MIN 10000

#define BM_BINARY_GT 10001
#define BM_BINARY_GE 10002
#define BM_BINARY_LT 10003
#define BM_BINARY_LE 10004
#define BM_BINARY_EQ 10005
#define BM_BINARY_NE 10006
#define BM_BINARY_SQUARED_DIFF 10007
#define BM_BINARY_FLOOR_MOD 10008
#define BM_BINARY_FLOOR_DIV 10009

#define TENSOR_N_DIM 0
#define TENSOR_C_DIM 1
#define TENSOR_H_DIM 2
#define TENSOR_W_DIM 3

#endif /* OP_CODE_H_ */