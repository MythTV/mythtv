
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
__kernel
void videoHistogram64(__read_only image2d_t fTop, __read_only image2d_t fBot,
                      __local uint *histLoc, __global uint *histOut,
                      int2 total)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    int outidx = 64 * (get_group_id(0) + (get_group_id(1) * get_num_groups(0)));
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    uint4 maxval = (uint4)(3, 3, 3, 0);

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            histLoc[i] = 0;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        float4 pixelT = read_imagef(fTop, sampler, (int2)(x, y));
        float4 pixelB = read_imagef(fBot, sampler, (int2)(x, y));
        uint4 offsetT = min(convert_uint4(pixelT * 4.0), maxval);
        uint4 offsetB = min(convert_uint4(pixelB * 4.0), maxval);

        uint indexT = (offsetT.x << 4) | (offsetT.y << 2) | (offsetT.z);
        uint indexB = (offsetB.x << 4) | (offsetB.y << 2) | (offsetB.z);

        atom_inc( histLoc + indexT );
        atom_inc( histLoc + indexB );
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            histOut[outidx + i] = histLoc[i];
        }
    }
}

// Reduce the array down to a single entity (need two runs)
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
__kernel
void videoHistogramReduce(__global uint *histIn, __local uint *histLoc,
                          __global uint *histOut)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    int rowsize = get_global_size(0);

    int inIndex = 64 * (x + (y * rowsize));
    int outIndex = 64 * y;

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            histLoc[i] = 0;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    for (int i = 0; i < 64; i++)
    {
        uint val = histIn[inIndex + i];

        atom_add( histLoc + i, val );
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            histOut[outIndex + i] = histLoc[i];
        }
    }
}

__kernel
void videoHistogramNormalize(__global uint *histIn, uint pixelCount,
                             __global float *histOut)
{
    int x = get_global_id(0);

    histOut[x] = convert_float(histIn[x]) / convert_float(pixelCount);
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
