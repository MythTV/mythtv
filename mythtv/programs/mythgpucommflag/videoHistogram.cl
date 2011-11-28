
#define HIST_CHUNK (2.0 / 64.0)

#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
__kernel
void videoHistogram64(__read_only image2d_t fTop, __read_only image2d_t fBot,
                      __local uint *histLoc, __global uint4 *histOut,
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
    uint4 maxval = (uint4)(63, 63, 63, 0);

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64 * 3; i++)
        {
            histLoc[i] = 0;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        float4 pixelT = read_imagef(fTop, sampler, (int2)(x, y));
        float4 pixelB = read_imagef(fBot, sampler, (int2)(x, y));
        uint4 offsetT = min(convert_uint4((pixelT + 1.0) / HIST_CHUNK), maxval);
        uint4 offsetB = min(convert_uint4((pixelB + 1.0) / HIST_CHUNK), maxval);

        atom_inc( histLoc + (3 * offsetT.x) );
        atom_inc( histLoc + (3 * offsetT.y) + 1 );
        atom_inc( histLoc + (3 * offsetT.z) + 2 );

        atom_inc( histLoc + (3 * offsetB.x) );
        atom_inc( histLoc + (3 * offsetB.y) + 1 );
        atom_inc( histLoc + (3 * offsetB.z) + 2 );
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            uint4 outval;
            outval.x = histLoc[3 * i];
            outval.y = histLoc[(3 * i) + 1];
            outval.z = histLoc[(3 * i) + 2];
            outval.w = 0;
            histOut[outidx + i] = outval;
        }
    }
}

// Reduce the array down to a single entity (need two runs)
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
__kernel
void videoHistogramReduce(__global uint4 *histIn, __local uint *histLoc,
                          __global uint4 *histOut)
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
        for (int ii = 0; ii < 64 * 3; ii++)
        {
            histLoc[ii] = 0;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    for (int ii = 0; ii < 64; ii++)
    {
        uint4 val = histIn[inIndex + ii];

        atom_add( histLoc + (3 * ii), val.x );
        atom_add( histLoc + (3 * ii) + 1, val.y );
        atom_add( histLoc + (3 * ii) + 2, val.z );
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0 && ly == 0)
    {
        for (int ii = 0; ii < 64; ii++)
        {
            uint4 outval;
            outval.x = histLoc[3 * ii];
            outval.y = histLoc[(3 * ii) + 1];
            outval.z = histLoc[(3 * ii) + 2];
            outval.w = 0;
            histOut[outIndex + ii] = outval;
        }
    }
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
