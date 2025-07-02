#ifndef QUICKSELECT_H
#define QUICKSELECT_H

template <typename T>
void ELEM_SWAP(T& a, T& b)
{
    T t = a;
    a = b;
    b = t;
}

template <typename T>
T quick_select(T *arr, int nelems, int select)
{
    int low = 0;
    int high = nelems - 1;

    for (;;) {
        if (high <= low) /* One element only */
            return arr[select];

        if (high == low + 1) {  /* Two elements only */
            if (arr[low] > arr[high])
                ELEM_SWAP<T>(arr[low], arr[high]);
            return arr[select];
        }

        /* Find median of low, middle and high items; swap into position low */
        int middle = (low + high) / 2;
        if (arr[middle] > arr[high])    ELEM_SWAP<T>(arr[middle], arr[high]);
        if (arr[low]    > arr[high])    ELEM_SWAP<T>(arr[low],    arr[high]);
        if (arr[middle] > arr[low])     ELEM_SWAP<T>(arr[middle], arr[low]);

        /* Swap low item (now in position middle) into position (low+1) */
        ELEM_SWAP<T>(arr[middle], arr[low+1]);

        /* Nibble from each end towards middle, swapping items when stuck */
        int ll = low + 1;
        int hh = high;
        while (hh >= ll) {
            ll++;
            while (arr[low] > arr[ll])
                ll++;

            hh--;
            while (arr[hh] > arr[low])
                hh--;

            if (hh < ll)
                break;

            ELEM_SWAP<T>(arr[ll], arr[hh]);
        }

        /* Swap middle item (in position low) back into correct position */
        ELEM_SWAP<T>(arr[low], arr[hh]);

        /* Re-set active partition */
        if (hh <= select)
            low = ll;
        if (hh >= select)
            high = hh - 1;
    }
}

template <typename T>
T quick_select_median(T *arr, int nelems)
{
    return quick_select<T>(arr, nelems, (nelems - 1) / 2);
}

#endif  /* !QUICKSELECT_H */
