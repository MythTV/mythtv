// Pack the YUV420P planes into one image
__kernel
void videoCombineYUV(__read_only image2d_t Y, __read_only image2d_t UV,
                     __write_only image2d_t YUV)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    float4 val;

    val.x  = read_imagef(Y,  sampler, (int2)(x, y)).x;
    val.yz = read_imagef(UV, sampler, (int2)(x / 2, y / 2)).xy;
    val.yz -= 0.5;  // VDPAU->OpenGL->OpenCL packs as CL_UNORM_8, need SNORM
    val.yz *= 2.0;
    val.w  = 1.0;

    write_imagef(YUV, (int2)(x, y), val);
}

// Pack the YUV420P planes into one image, split into two fields
__kernel
void videoCombineFFMpegYUV(__read_only image2d_t in,
                           __write_only image2d_t top,
                           __write_only image2d_t bot)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    float4 val;

    int maxx = get_global_size(0);
    int maxy = get_global_size(1);

    int pixnumy  = x + (y * maxx);
    int pixnumuv = (x / 2) + ((y / 2) * (maxx / 2));
    int xuv = pixnumuv % maxx;
    int yuv = pixnumuv / maxx;

    int2 ypos = (int2)(x, y);
    int2 upos = (int2)(xuv, maxy + yuv);
    int2 vpos = (int2)(xuv, (maxy * 5) / 4 + yuv);

    val.x = read_imagef(in, sampler, ypos).x;
    val.y = read_imagef(in, sampler, upos).x;
    val.z = read_imagef(in, sampler, vpos).x;
    val.yz -= 0.5;  // FFMpeg packs as CL_UNORM_8, need SNORM
    val.yz *= 2.0;
    val.w  = 1.0;

    if (y % 2)
        write_imagef(bot, (int2)(x, y / 2), val);
    else
        write_imagef(top, (int2)(x, y / 2), val);
}


// Convert the YUV for use with SNORM buffers (for dumping)
__kernel
void videoYUVToSNORM(__read_only image2d_t in, __write_only image2d_t out)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    float4 val;

    val = read_imagef(in,  sampler, (int2)(x, y));
    val.x -= 0.5;
    val.x *= 2.0;
    val.w  = 1.0;

    write_imagef(out, (int2)(x, y), val);
}

// Convert the YUV from use with SNORM buffers (for dumping)
__kernel
void videoYUVFromSNORM(__read_only image2d_t in, __write_only image2d_t out)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    float4 val;

    val = read_imagef(in,  sampler, (int2)(x, y));
    val.x += 1.0;
    val.x /= 2.0;
    val.w  = 1.0;

    write_imagef(out, (int2)(x, y), val);
}


// Convert Packed YUV into RGB
__kernel
void videoYUVToRGB(__read_only image2d_t YUV, __write_only image2d_t RGB)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    float4 yuvval;
    float4 rgbval;

    yuvval = read_imagef(YUV,  sampler, (int2)(x, y));

    rgbval.x = yuvval.x + (1.13983 * yuvval.z);
    rgbval.y = yuvval.x + (-0.39465 * yuvval.y) + (-0.58060 * yuvval.z);
    rgbval.z = yuvval.x + (2.03211 * yuvval.y);
    rgbval.w = 1.0;

    write_imagef(RGB, (int2)(x, y), rgbval);
}

// Zero a region of a buffer
__kernel
void videoZeroRegion(__read_only image2d_t in, __write_only image2d_t out,
                     int2 regionStart, int2 regionEnd)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    float4 output;

    if (x >= regionStart.x && x < regionEnd.x &&
        y >= regionStart.y && y < regionEnd.y)
    {
        output = 0.0;
        output.x = -1.0;    // since we are in SNORM mode.
    }
    else
    {
        output = read_imagef(in,  sampler, (int2)(x, y));
    }

    output.w = 1.0;

    write_imagef(out, (int2)(x, y), output);
}

// Threshold => saturate
__kernel
void videoThreshSaturate(__read_only image2d_t in, __write_only image2d_t out,
                         int threshold)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    float4 output;
    float thresh = ((convert_float(threshold) / 255.0) - 0.5) * 2.0;
    float thresh2 = convert_float(threshold) / 255.0;

    output = read_imagef(in,  sampler, (int2)(x, y));

    output.x = (ceil(fmax(output.x - thresh, 0.0)) - 0.5) * 2.0;
    output.y = ceil(fmax(fabs(output.y) - thresh2, 0.0));
    output.z = ceil(fmax(fabs(output.z) - thresh2, 0.0));
    output.w = 1.0;

    write_imagef(out, (int2)(x, y), output);
}

__kernel
void videoCopyLogoROI(__read_only image2d_t in, __write_only image2d_t out,
                      int2 regionStart, int2 regionEnd)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    float4 output;

    int xover = min(x / regionStart.x, 1);
    int yover = min(y / regionStart.y, 1);
    int xin = x + (xover * (regionEnd.x - regionStart.x));
    int yin = y + (yover * (regionEnd.y - regionStart.y));

    output = read_imagef(in,  sampler, (int2)(xin, yin));
    write_imagef(out, (int2)(x, y), output);
}

__kernel
void videoLogoMSE(__read_only image2d_t ref, __read_only image2d_t in,
                  __write_only image2d_t out)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    float4 in1;
    float4 in2;
    float4 output;

    in1 = read_imagef(ref, sampler, (int2)(x, y));
    in2 = read_imagef(in, sampler, (int2)(x, y));
    output = 1.0;
    output.x = in2.x - in1.x;
    output.x *= output.x * 2.0;
    output.x -= 1.0;
    write_imagef(out, (int2)(x, y), output);
}

__kernel
void videoMultiply(__read_only image2d_t ref, __read_only image2d_t in,
                   __write_only image2d_t out)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;
    float4 in1;
    float4 in2;
    float4 output;

    in1 = (read_imagef(ref, sampler, (int2)(x, y)) + 1.0) / 2.0;
    in2 = (read_imagef(in, sampler, (int2)(x, y)) + 1.0) / 2.0;
    output = 0.0;
    output.x = ((in2.x * in1.x) * 2.0) - 1.0;
    write_imagef(out, (int2)(x, y), output);
}

__kernel
void videoDilate3x3(__read_only image2d_t in, __write_only image2d_t out)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP |
                              CLK_FILTER_NEAREST;
    float4 output = -1.0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            int locx = x + i;
            int locy = y + j;

            output = fmax(output, read_imagef(in, sampler, (int2)(locx, locy)));
        }
    }
    output.yz = 0.0;
    output.w  = 1.0;

    write_imagef(out, (int2)(x, y), output);
}



/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
