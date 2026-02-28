#include <stdio.h>
#include "bitarray.h"

int set(int a,int pos)
{
    int m  = 1 << pos;
    return (a | m);
}

int invert_bit(int a,int pos)
{
    int m  = 1 << pos;
    return (a ^ m);
}

int test_bit(int a, int pos)
{
    int m = 1 << pos;
    return ((a & m) == m);
}

int is_one_bit(int a)
{
    return a>0 && (a&(a-1))==0;
}

int shift_right(int a,int pos)
{
    return a >> pos;
}

int shift_left(int a, int pos)
{
    return a << pos;
}

// Wegner popcount
int popcount(int  x)
{
    int count;
    for (count=0; x; count++)
        x &= x - 1;
    return count;
}

int outer_bit_pos(int a)
{
    int mask;
    shift_func f;
    int lt_half = a < shift_left(1,BITARRAY_SIZE/2);
        
    if (a == 0)
        return -1;

    if (lt_half){
        f = &shift_right;
        mask = 1;
    }
    else
    {
        f = &shift_left;
        mask = shift_left(1,BITARRAY_SIZE-1);
    }

    for(int i=0;i<BITARRAY_SIZE;++i){
        if ((mask & (*f)(a,i)) == mask)
            return lt_half?i:(BITARRAY_SIZE-i);
    }
}



