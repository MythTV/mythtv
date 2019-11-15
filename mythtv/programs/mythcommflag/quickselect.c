/*
 *  http://ndevilla.free.fr/median/median/src/quickselect.c
 *
 *  This Quickselect routine is based on the algorithm described in
 *  "Numerical recipes in C", Second Edition,
 *  Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
 *  This code by Nicolas Devillard - 1998. Public domain.
 */

#include "quickselect.h"

#define ELEM_SWAP(a,b) { register unsigned char t=(a);(a)=(b);(b)=t; }

unsigned char
quick_select(unsigned char *arr, int nelems, int select)
{
    int low = 0;
    int high = nelems - 1;

    for (;;) {
        if (high <= low) /* One element only */
            return arr[select];

        if (high == low + 1) {  /* Two elements only */
            if (arr[low] > arr[high])
                ELEM_SWAP(arr[low], arr[high]);
            return arr[select];
        }

        /* Find median of low, middle and high items; swap into position low */
        int middle = (low + high) / 2;
        if (arr[middle] > arr[high])    ELEM_SWAP(arr[middle], arr[high]);
        if (arr[low]    > arr[high])    ELEM_SWAP(arr[low],    arr[high]);
        if (arr[middle] > arr[low])     ELEM_SWAP(arr[middle], arr[low]);

        /* Swap low item (now in position middle) into position (low+1) */
        ELEM_SWAP(arr[middle], arr[low+1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        int ll = low + 1;
        int hh = high;
        for (;;) {
            do ll++; while (arr[low] > arr[ll]);
            do hh--; while (arr[hh]  > arr[low]);

            if (hh < ll)
                break;

            ELEM_SWAP(arr[ll], arr[hh]);
        }

        /* Swap middle item (in position low) back into correct position */
        ELEM_SWAP(arr[low], arr[hh]);

        /* Re-set active partition */
        if (hh <= select)
            low = ll;
        if (hh >= select)
            high = hh - 1;
    }
}

#undef ELEM_SWAP

unsigned char
quick_select_median(unsigned char *arr, int nelems)
{
    return quick_select(arr, nelems, (nelems - 1) / 2);
}

#define ELEM_SWAP(a,b) { register unsigned short t=(a);(a)=(b);(b)=t; }

unsigned short
quick_select_ushort(unsigned short *arr, int nelems, int select)
{
    int low = 0;
    int high = nelems - 1;

    for (;;) {
        if (high <= low) /* One element only */
            return arr[select];

        if (high == low + 1) {  /* Two elements only */
            if (arr[low] > arr[high])
                ELEM_SWAP(arr[low], arr[high]);
            return arr[select];
        }

        /* Find median of low, middle and high items; swap into position low */
        int middle = (low + high) / 2;
        if (arr[middle] > arr[high])    ELEM_SWAP(arr[middle], arr[high]);
        if (arr[low]    > arr[high])    ELEM_SWAP(arr[low],    arr[high]);
        if (arr[middle] > arr[low])     ELEM_SWAP(arr[middle], arr[low]);

        /* Swap low item (now in position middle) into position (low+1) */
        ELEM_SWAP(arr[middle], arr[low+1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        int ll = low + 1;
        int hh = high;
        for (;;) {
            do ll++; while (arr[low] > arr[ll]);
            do hh--; while (arr[hh]  > arr[low]);

            if (hh < ll)
                break;

            ELEM_SWAP(arr[ll], arr[hh]);
        }

        /* Swap middle item (in position low) back into correct position */
        ELEM_SWAP(arr[low], arr[hh]);

        /* Re-set active partition */
        if (hh <= select)
            low = ll;
        if (hh >= select)
            high = hh - 1;
    }
}

#undef ELEM_SWAP

unsigned short
quick_select_median_ushort(unsigned short *arr, int nelems)
{
    return quick_select_ushort(arr, nelems, (nelems - 1) / 2);
}

#define ELEM_SWAP(a,b) { register float t=(a);(a)=(b);(b)=t; }

float
quick_select_float(float *arr, int nelems, int select)
{
    int low = 0;
    int high = nelems - 1;

    for (;;) {
        if (high <= low) /* One element only */
            return arr[select];

        if (high == low + 1) {  /* Two elements only */
            if (arr[low] > arr[high])
                ELEM_SWAP(arr[low], arr[high]);
            return arr[select];
        }

        /* Find median of low, middle and high items; swap into position low */
        int middle = (low + high) / 2;
        if (arr[middle] > arr[high])    ELEM_SWAP(arr[middle], arr[high]);
        if (arr[low]    > arr[high])    ELEM_SWAP(arr[low],    arr[high]);
        if (arr[middle] > arr[low])     ELEM_SWAP(arr[middle], arr[low]);

        /* Swap low item (now in position middle) into position (low+1) */
        ELEM_SWAP(arr[middle], arr[low+1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        int ll = low + 1;
        int hh = high;
        for (;;) {
            do ll++; while (arr[low] > arr[ll]);
            do hh--; while (arr[hh]  > arr[low]);

            if (hh < ll)
                break;

            ELEM_SWAP(arr[ll], arr[hh]);
        }

        /* Swap middle item (in position low) back into correct position */
        ELEM_SWAP(arr[low], arr[hh]);

        /* Re-set active partition */
        if (hh <= select)
            low = ll;
        if (hh >= select)
            high = hh - 1;
    }
}

float
quick_select_median_float(float *arr, int nelems)
{
    return quick_select_float(arr, nelems, (nelems - 1) / 2);
}

#undef ELEM_SWAP

/* vim: set expandtab tabstop=4 shiftwidth=4: */
