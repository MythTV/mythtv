// Perform a 2D Haar Discrete Wavelet Transform
__kernel
void videoWavelet(__read_only image2d_t fIn, __write_only image2d_t fOut,
                  int width, int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    int halfx = width / 2;
    int halfy = height / 2;
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    if (x >= 0 && x < width && y >= 0 && y < height)
    {
        int isDx = x / halfx;
        int isDy = y / halfy;

        int xin = (x - (halfx * isDx)) * 2; 
        int yin = (y - (halfy * isDy)) * 2;;

        float4 val;
        val.x = read_imagef(fIn, sampler, (int2)(xin, yin)).x;
        val.y = read_imagef(fIn, sampler, (int2)(xin, yin + 1)).x;
        val.z = read_imagef(fIn, sampler, (int2)(xin + 1, yin)).x;
        val.w = read_imagef(fIn, sampler, (int2)(xin + 1, yin + 1)).x;

        val.zw *= pown(-1.0, isDx);

        float2 outval;
        outval = M_SQRT1_2 * (val.xy + val.zw);
        outval.y *= pown(-1.0, isDy);

        float4 output;
        output = M_SQRT1_2 * (outval.x + outval.y);

        write_imagef(fOut, (int2)(x, y), output);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
