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


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
