// Generate the end stats from the reduced data
#pragma OPENCL EXTENSION cl_khr_fp64: enable
__kernel
void audioVolumeLevelStats64(__global long4  *resultsIn,
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

        double4 fResIn = convert_double4(resIn);
        double4 fIntSt = convert_double4(intSt);
        double4 fResGl = convert_double4(resGlbl);

        /*
          * fltStats.x = windowRMS
          * fltStats.y = globalRMS
          * fltStats.z = windowDNR
          */
        double RMS = sqrt(fResGl.x * exp2(fIntSt.x) / fResGl.z);
        if (RMS == 0.0)
            RMS = 1.0;
        fltSt.y = 20.0 * log10(RMS / convert_double(SHRT_MAX));

        RMS = sqrt(fResIn.x * exp2(fIntSt.x) / fResIn.z);
        if (RMS == 0.0)
            RMS = 1.0;
        fltSt.x = 20.0 * log10(RMS / convert_double(SHRT_MAX));

        fltSt.z = 20.0 * log10(fResGl.w / RMS);

        resultsGlobal[0] = resGlbl;
        intStats[0] = intSt;
        fltStats[0] = fltSt;
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
