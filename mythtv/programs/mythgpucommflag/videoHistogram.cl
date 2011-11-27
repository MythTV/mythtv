#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable

#define HIST_CHUNK (2.0 / 64.0)

__kernel
void videoHistogram64(__read_only image2d_t fTop, __read_only image2d_t fBot,
                      __write_only uint4 *histOut, int2 total)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    int outidx = 64 * (get_group_id(0) + (get_group_id(1) * get_num_groups(0)));

    __local uint localHistY[64];
    __local uint localHistU[64];
    __local uint localHistV[64];

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            localHistY[i] = 0;
            localHistU[i] = 0;
            localHistV[i] = 0;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        float4 pixelT = read_imagef(fTop, sampler, (int2)(x, y));
        float4 pixelB = read_imagef(fBot, sampler, (int2)(x, y));
        uint4 offsetT = convert_uint4((pixelT + 1.0) / HIST_CHUNK) - 1;
        uint4 offsetB = convert_uint4((pixelB + 1.0) / HIST_CHUNK) - 1;

        atom_inc( &localHistY[offsetT.x] );
        atom_inc( &localHistU[offsetT.y] );
        atom_inc( &localHistV[offsetT.z] );

        atom_inc( &localHistY[offsetB.x] );
        atom_inc( &localHistU[offsetB.y] );
        atom_inc( &localHistV[offsetB.z] );
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0 && ly == 0)
    {
        for (int i = 0; i < 64; i++)
        {
            int4 outval;
            outval.x = localHistY[i];
            outval.y = localHistU[i];
            outval.z = localHistV[i];
            outval.w = 0;
            histOut[outidx + i] = outval;
        }
    }
}

// Reduce the array down to a single entity
__kernel
void videoHistogramReduce(__global uint4 *histIn, __global uint4 *histOut,
                          int2 total)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int rowsize = get_global_size(0) * 2;

    int i = 64 * (x + (y * rowsize));
    int j = i + (64 * get_global_size(0));
    int k = i + (64 * get_global_size(1) * rowsize);
    int l = k + (64 * get_global_size(0));

    __local uint localHistY[64];
    __local uint localHistU[64];
    __local uint localHistV[64];

    if (x == 0 && y == 0)
    {
        for (int ii = 0; ii < 64; ii++)
        {
            localHistY[ii] = 0;
            localHistU[ii] = 0;
            localHistV[ii] = 0;
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    for (int ii = 0; ii < 64; ii++)
    {
        uint4 val = histIn[i + ii];

        if (x + get_global_size(0) < total.x)
            val += histIn[j + ii];

        if (y + get_global_size(1) < total.y)
        {
            val += histIn[k + ii];

            if (x + get_global_size(0) < total.x)
                val += histIn[l + ii];
        }

        atom_add( &localHistY[ii], val.x );
        atom_add( &localHistU[ii], val.y );
        atom_add( &localHistV[ii], val.z );
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (x == 0 && y == 0)
    {
        for (int ii = 0; ii < 64; ii++)
        {
            int4 outval;
            outval.x = localHistY[ii];
            outval.y = localHistU[ii];
            outval.z = localHistV[ii];
            outval.w = 0;
            histOut[ii] = outval;
        }
    }
}



/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
