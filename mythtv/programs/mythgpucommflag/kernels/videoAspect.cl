
__kernel
void videoFindPillarBox(__read_only image2d_t fTop, __read_only image2d_t fBot,
                        int2 total, int2 box, __local uchar2 *histLoc,
                        __global uchar *outL, __global uchar *outR)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int x2 = total.x - x - 1;
    int maxx = get_global_size(0);

    int lx = get_local_id(0);
    int ly = get_local_id(1);
    int lmaxx = get_local_size(0);
    int lmaxy = get_local_size(1);

    int gy = get_group_id(1);

    int outidx = x + (gy * maxx);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    uint4 maxval = (uint4)(3, 3, 3, 0);


    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        int locidx = lx + (ly * lmaxx);
        float4 pixelTL = read_imagef(fTop, sampler, (int2)(x, y));
        float4 pixelBL = read_imagef(fBot, sampler, (int2)(x, y));
        uint4 offsetTL = min(convert_uint4(pixelTL * 4.0), maxval);
        uint4 offsetBL = min(convert_uint4(pixelBL * 4.0), maxval);
        int indexTL = (offsetTL.x << 4) | (offsetTL.y << 2) | (offsetTL.z);
        int indexBL = (offsetBL.x << 4) | (offsetBL.y << 2) | (offsetBL.z);
        int valL = max(1 - indexTL, 0) + max(1 - indexBL, 0);
        histLoc[locidx].x = convert_uchar(valL);

        int valR = 1;
        if (x2 >= 0)
        {
            float4 pixelTR = read_imagef(fTop, sampler, (int2)(x2, y));
            float4 pixelBR = read_imagef(fBot, sampler, (int2)(x2, y));
            uint4 offsetTR = min(convert_uint4(pixelTR * 4.0), maxval);
            uint4 offsetBR = min(convert_uint4(pixelBR * 4.0), maxval);
            int indexTR = (offsetTR.x << 4) | (offsetTR.y << 2) | (offsetTR.z);
            int indexBR = (offsetBR.x << 4) | (offsetBR.y << 2) | (offsetBR.z);
            valR = max(1 - indexTR, 0) + max(1 - indexBR, 0);
        }
        histLoc[locidx].y = convert_uchar(valR);
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (ly == 0)
    {
        uchar sumL = 0;
        uchar sumR = 0;
        for (int i = 0; i < lmaxy; i++)
        {
            int locidx = lx + (i * lmaxx);
            sumL += histLoc[locidx].x;
            sumR += histLoc[locidx].y;
        }
        outL[outidx] = sumL;
        outR[outidx] = sumR;
    }
}


__kernel
void videoPillarReduceVert(__global uchar *inL, __global uchar *inR,
                           int2 total, int2 box, __local ushort2 *locData,
                           __global ushort *outL, __global ushort *outR)
{
    int x = get_global_id(0);
    int lx = get_local_id(0);
    int lmaxx = get_local_size(0);
    int gx = get_group_id(0);

    ushort sumL = 0;
    ushort sumR = 0;

    for (int i = 0; i < box.y; i++)
    {
        int idx = x + (i * box.x);
        sumL += convert_ushort(inL[idx]);
        sumR += convert_ushort(inR[idx]);
    }

    uint thresh = convert_uint(convert_float(total.y) * 1.80);

    locData[lx].x = sumL / thresh;
    locData[lx].y = sumR / thresh;

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0)
    {
        bool done = false;
        int i;
        for (i = 0; i < lmaxx && locData[i].x; i++);
        outL[gx] = i;

        for (i = 0; i < lmaxx && locData[i].y; i++);
        outR[gx] = i;
    }
}

__kernel
void videoPillarReduceHoriz(__global ushort *inL, __global ushort *inR,
                            int2 total, int groupsize, __global uint *cols)
{
    int x = get_global_id(0);
    int maxx = get_global_size(0);
    ushort full = convert_ushort(groupsize);

    if (x == 0)
    {
        int count = 0;
        int i = 0;
        do {
            count += inL[i];
        } while (inL[i++] == full && i < maxx);
        cols[0] = count;

        count = 0;
        i = 0;
        do {
            count += inR[i];
        } while (inR[i++] == full && i < maxx);
        cols[1] = total.x - count - 1;
    }
}


__kernel
void videoFindLetterBox(__read_only image2d_t fTop, __read_only image2d_t fBot,
                        int2 total, int2 box, __local uchar2 *histLoc,
                        __global uchar *outT, __global uchar *outB)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int y2 = total.y - y - 1;

    int lx = get_local_id(0);
    int lmaxx = get_local_size(0);
    int ly = get_local_id(1);

    int gx = get_group_id(0);
    int gmaxx = get_num_groups(0);

    int outidx = gx + (y * gmaxx);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    uint4 maxval = (uint4)(3, 3, 3, 0);


    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        int locidx = lx + (ly * lmaxx);
        float4 pixelTT = read_imagef(fTop, sampler, (int2)(x, y));
        float4 pixelBT = read_imagef(fBot, sampler, (int2)(x, y));
        uint4 offsetTT = min(convert_uint4(pixelTT * 4.0), maxval);
        uint4 offsetBT = min(convert_uint4(pixelBT * 4.0), maxval);
        int indexTT = (offsetTT.x << 4) | (offsetTT.y << 2) | (offsetTT.z);
        int indexBT = (offsetBT.x << 4) | (offsetBT.y << 2) | (offsetBT.z);
        int valT = max(1 - indexTT, 0) + max(1 - indexBT, 0);
        histLoc[locidx].x = convert_uchar(valT);

        int valB = 1;
        if (y2 >= 0)
        {
            float4 pixelTB = read_imagef(fTop, sampler, (int2)(x, y2));
            float4 pixelBB = read_imagef(fBot, sampler, (int2)(x, y2));
            uint4 offsetTB = min(convert_uint4(pixelTB * 4.0), maxval);
            uint4 offsetBB = min(convert_uint4(pixelBB * 4.0), maxval);
            int indexTB = (offsetTB.x << 4) | (offsetTB.y << 2) | (offsetTB.z);
            int indexBB = (offsetBB.x << 4) | (offsetBB.y << 2) | (offsetBB.z);
            valB = max(1 - indexTB, 0) + max(1 - indexBB, 0);
        }
        histLoc[locidx].y = convert_uchar(valB);
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (lx == 0)
    {
        uchar sumT = 0;
        uchar sumB = 0;
        for (int i = 0; i < lmaxx; i++)
        {
            int locidx = i + (ly * lmaxx);
            sumT += histLoc[locidx].x;
            sumB += histLoc[locidx].y;
        }
        outT[outidx] = sumT;
        outB[outidx] = sumB;
    }
}


__kernel
void videoLetterReduceHoriz(__global uchar *inT, __global uchar *inB,
                            int2 total, int2 box, __local ushort2 *locData,
                            __global ushort *outT, __global ushort *outB)
{
    int y = get_global_id(0);
    int ly = get_local_id(0);
    int lmaxy = get_local_size(0);
    int gy = get_group_id(0);

    ushort sumT = 0;
    ushort sumB = 0;

    for (int i = 0; i < box.x; i++)
    {
        int idx = i + (y * box.x);
        sumT += convert_ushort(inT[idx]);
        sumB += convert_ushort(inB[idx]);
    }

    // Doubled since we combined the fields
    uint thresh = convert_uint(convert_float(total.x) * 1.80);

    locData[ly].x = sumT / thresh;
    locData[ly].y = sumB / thresh;

    barrier(CLK_LOCAL_MEM_FENCE);

    if (ly == 0)
    {
        int i;
        for (i = 0; i < lmaxy && locData[i].x; i++);
        outT[gy] = i;

        for (i = 0; i < lmaxy && locData[i].y; i++);
        outB[gy] = i;
    }
}

__kernel
void videoLetterReduceVert(__global ushort *inT, __global ushort *inB,
                           int2 total, int groupsize, __global uint *rows)
{
    int y = get_global_id(0);
    int maxy = get_global_size(0);
    ushort all = convert_ushort(groupsize);

    if (y == 0)
    {
        int count = 0;
        int i = 0;
        do {
            count += inT[i];
        } while (inT[i++] == all && i < maxy);
        rows[0] = count;

        count = 0;
        i = 0;
        do {
            count += inB[i];
        } while (inB[i++] == all && i < maxy);
        rows[1] = total.y - count - 1;
    }
}


__kernel
void videoCrop(__read_only image2d_t fInTop, __read_only image2d_t fInBot,
               uint2 topL, uint2 botR,
               __write_only image2d_t fOutTop, __write_only image2d_t fOutBot)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    if (x >= topL.x && x <= botR.x && y >= topL.y && y <= botR.y)
    {
        float4 valT = read_imagef(fInTop, sampler, (int2)(x, y));
        float4 valB = read_imagef(fInBot, sampler, (int2)(x, y));

        write_imagef(fOutTop, (int2)(x - topL.x, y - topL.y), valT);
        write_imagef(fOutBot, (int2)(x - topL.x, y - topL.y), valB);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
