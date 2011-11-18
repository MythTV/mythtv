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

        float4 val[4];
        val[0] = read_imagef(fIn, sampler, (int2)(xin, yin));
        val[1] = read_imagef(fIn, sampler, (int2)(xin, yin + 1));
        val[2] = read_imagef(fIn, sampler, (int2)(xin + 1, yin));
        val[3] = read_imagef(fIn, sampler, (int2)(xin + 1, yin + 1));

        float sign = pown(-1.0, isDx);
        val[2] *= sign;
        val[3] *= sign;

        float4 outval[2];
        outval[0] = M_SQRT1_2 * (val[0] + val[2]);
        outval[1] = M_SQRT1_2 * (val[1] + val[3]);

        sign = pown(-1.0, isDy);
        outval[1] *= sign;

        float4 output;
        output = M_SQRT1_2 * (outval[0] + outval[1]);

        write_imagef(fOut, (int2)(x, y), output);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
