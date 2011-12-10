// Mix down to one channel
__kernel
void audioVolumeLevelMix(__global __read_only short *samplesIn,
                         __global long4 *results)
{
    /*
     * results.x = accum
     * results.y = accumWindowDC
     * results.z = count
     * results.w = maxsample 
     */
    int x = get_global_id(0);

    if (x == 0)
    {
        int y = get_global_id(1);
        int xsize = get_global_size(0);
        long4 out = 0;
        int index = y * xsize;

        for (int i = 0; i < xsize; i++)
        {
            short sample = samplesIn[index + i];
            out.w = max(out.w, convert_long(sample));
            out.x += (sample * sample);
            out.y += sample;
        }
        out.z = xsize;
        results[y] = out;
    }
}

// Reduce the array down to a single entity
__kernel
void audioVolumeLevelReduce(__global long4 *resultsIn,
                            __global long4 *resultsOut,
                            __local volatile long4 *localRes,
                            int n, int blockSize)
{
    // perform first level of reduction,
    // reading from global memory, writing to shared memory
    unsigned int tid = get_local_id(0);
    unsigned int i = get_group_id(0)*(get_local_size(0)*2) + get_local_id(0);
    unsigned int j = i + get_local_size(0);

    localRes[tid] = (i < n) ? resultsIn[i] : 0;
    if (j < n)
    {
        localRes[tid].xyz += resultsIn[j].xyz;
        localRes[tid].w = max(localRes[tid].w, resultsIn[j].w);
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // do reduction in shared mem
    #pragma unroll 1
    for(unsigned int s=get_local_size(0)/2; s>32; s>>=1)
    {   
        if (tid < s)
        {   
            localRes[tid].xyz += resultsIn[tid + s].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid + s].w);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if (tid < 32)
    {   
        if (blockSize >=  64)
        {
            localRes[tid].xyz += resultsIn[tid + 32].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid + 32].w);
        }

        if (blockSize >=  32)
        {
            localRes[tid].xyz += resultsIn[tid + 16].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid + 16].w);
        }

        if (blockSize >=  16)
        {
            localRes[tid].xyz += resultsIn[tid +  8].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid +  8].w);
        }

        if (blockSize >=   8)
        {
            localRes[tid].xyz += resultsIn[tid +  4].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid +  4].w);
        }

        if (blockSize >=   4)
        {
            localRes[tid].xyz += resultsIn[tid +  2].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid +  2].w);
        }

        if (blockSize >=   2)
        {
            localRes[tid].xyz += resultsIn[tid +  1].xyz;
            localRes[tid].w = max(localRes[tid].w, resultsIn[tid +  1].w);
        }
    }

    // write result for this block to global mem
    if (tid == 0) resultsOut[get_group_id(0)] = localRes[0];
}


// Generate the end stats from the reduced data
__kernel
void audioVolumeLevelStats32(__global long4  *resultsIn,
                             __global long4  *resultsGlobal,
                             __global int4   *intStats,
                             __global float4 *fltStats)
{
    int i = get_global_id(0);

    if (i == 0)
    {
        /*
         * Results.x = accum
         * Results.y = accumWindowDC
         * Results.z = count
         * Results.w = maxsample (window)
         */
        long4  resGlbl = resultsGlobal[0];
        int4   intSt   = 0;
        long4  resIn   = 0;

        resGlbl.w = 0;

        for (int j = 0; j < get_global_size(0); j++)
        {
            long4  resInJ = resultsIn[j];

            resIn.yz += resInJ.yz;
            resGlbl.yz += resInJ.yz;
            if (resGlbl.x >= (LONG_MAX - resInJ.x))
            {
                intSt.x += 2;
                resGlbl.x >>= 2;
                resGlbl.x += (resInJ.x >> 2);
                resIn.x += (resInJ.x >> 2);
            }
            else
            {
                resGlbl.x += resInJ.x;
                resIn.x += resInJ.x;
            }
            resGlbl.w = max(resGlbl.w, resInJ.w);
            resIn.w = max(resIn.w, resInJ.w);
        }

        float4 fltSt   = fltStats[0];
        /*
         * intStats.x = globalShift
         * intStats,y = windowDC
         * intStats.z = globalDC
         */
        intSt.y = resIn.y / resIn.z;
        intSt.z = resGlbl.y / resGlbl.z;

        resGlbl.w -= intSt.z;
        if (resGlbl.w <= 0)
            resGlbl.w = 1;

        float4 fResIn = convert_float4(resIn);
        float4 fIntSt = convert_float4(intSt);
        float4 fResGl = convert_float4(resGlbl);

        /*
          * fltStats.x = windowRMS
          * fltStats.y = globalRMS
          * fltStats.z = windowDNR
          */
        float RMS = sqrt(fResGl.x * exp2(fIntSt.x) / fResGl.z);
        if (RMS == 0.0)
            RMS = 1.0;
        fltSt.y = 20.0 * log10(RMS / convert_float(SHRT_MAX));

        RMS = sqrt(fResIn.x * exp2(fIntSt.x) / fResIn.z);
        if (RMS == 0.0)
            RMS = 1.0;
        fltSt.x = 20.0 * log10(RMS / convert_float(SHRT_MAX));

        fltSt.z = 20.0 * log10(fResGl.w / RMS);

        resultsGlobal[0] = resGlbl;
        intStats[0] = intSt;
        fltStats[0] = fltSt;
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
