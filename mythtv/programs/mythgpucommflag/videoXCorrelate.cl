// Work groups are "n" in the summation for cross-correlation.
// There should be N workgroups, where N is the number of items in the 
// series to cross-correlate
__kernel
void videoCrossCorrelation(__global uint4 *a, __global uint4 *b,
                           __global ulong4 *out)
{
    int n = get_global_id(0);           // eg. 0-63
    int N = get_global_size(0);         // eg. 64

    // Do the a[n] * b[n - l] windowed for this value of n
    ulong4 locData = 0;

    for (int l = n; l < n + N; l++)
    {
        ulong4 aVal = convert_ulong4(a[n]);
        ulong4 bVal = convert_ulong4(b[N - 1 - l + n]);
        locData += aVal * bVal;
    }

    out[n] = locData;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
