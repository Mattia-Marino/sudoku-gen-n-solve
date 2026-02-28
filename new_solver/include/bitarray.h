#ifndef _BITARRAY_
#define _BITARRAY_
#endif

#define BITARRAY_SIZE sizeof(int)*8

typedef int (*shift_func)(int,int);

int set(int ,int );

int is_one_bit(int );

int shift_right(int,int );

int shift_left(int , int );

int outer_bit_pos(int );

int invert_bit(int ,int);

int test_bit(int , int);

int popcount(int );
