// Work groups are "l" in the summation for cross-correlation.
// There should be N workgroups, where N is the number of items in the 
// series to cross-correlate
__kernel
void videoCrossCorrelation(__global float *a, __global float *b,
                           __global float *out)
{
    int l = get_global_id(0);           // eg. 0-63
    int N = get_global_size(0);         // eg. 127
    int mid = ((N + 1) / 2) - 1;        // eg. 63
    float numN = convert_float(mid + 1);
    float mean = 1.0 / numN;

    // Do the a[n] * b[n - l] windowed for this value of l
    float locData = 0.0;

    for (int n = max(l - mid, 0); n <= min(l, mid); n++)
    {
        float aVal = a[n] - mean;
        float bVal = b[n - l + mid] - mean;
        locData += (aVal * bVal);
    }

    out[l] = locData;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
