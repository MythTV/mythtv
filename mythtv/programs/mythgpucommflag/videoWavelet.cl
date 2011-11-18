// Perform a 2D Haar Discrete Wavelet Transform
__kernel
void videoWavelet(__read_only image2d_t fIn, __write_only image2d_t fOut,
                  int2 total, int2 use)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        float4 output;

        if (x < use.x && y < use.y)
        {
            int middleX = use.x / 2;
            int middleY = use.y / 2;
            int isDx    = x / middleX;
            int isDy    = y / middleY;
            int locinX  = (x - (middleX * isDx)) * 2;
            int locinY  = (y - (middleY * isDy)) * 2;

            float4 a, b, c, d;
            a = read_imagef(fIn, sampler, (int2)(locinX,     locinY));
            b = read_imagef(fIn, sampler, (int2)(locinX + 1, locinY));
            c = read_imagef(fIn, sampler, (int2)(locinX,     locinY + 1));
            d = read_imagef(fIn, sampler, (int2)(locinX + 1, locinY + 1));

            float4 e, f, g, h;
            e = (a + b + c + d) / 4.0;
            f = (a - b + c - d) / 4.0;
            g = (a + b - c - d) / 4.0;
            h = (a - b - c + d) / 4.0;

            float4 outvals[4] = { e, f, g, h };
            int offset = isDx + (isDy << 1);

            output = outvals[offset];
            output.w = 1.0;
        }
        else
        {
            output = read_imagef(fIn, sampler, (int2)(x, y));
        }

        write_imagef(fOut, (int2)(x, y), output);
    }
}

// Perform a 2D Haar Inverse Discrete Wavelet Transform
__kernel
void videoWaveletInverse(__read_only image2d_t fIn, __write_only image2d_t fOut,
                         int2 total, int2 use)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    if (x >= 0 && x < total.x && y >= 0 && y < total.y)
    {
        float4 output;

        if (x < use.x && y < use.y)
        {
            int middleX = use.x / 2;
            int middleY = use.y / 2;
            int oddX    = x % 2;
            int oddY    = y % 2;
            int locinX  = x / 2;
            int locinY  = y / 2;

            float4 e, f, g, h;
            e = read_imagef(fIn, sampler, (int2)(locinX, locinY));
            f = read_imagef(fIn, sampler, (int2)(locinX + middleX, locinY));
            g = read_imagef(fIn, sampler, (int2)(locinX, locinY + middleY));
            h = read_imagef(fIn, sampler, (int2)(locinX + middleX,
                                                 locinY + middleY));

            float4 a, b, c, d;
            a = (e + f + g + h) / 1.0;
            b = (e - f + g - h) / 1.0;
            c = (e + f - g - h) / 1.0;
            d = (e - f - g + h) / 1.0;

            float4 outvals[4] = { a, b, c, d };
            int offset = oddX + (oddY << 1);

            output = outvals[offset];
            output.w = 1.0;
        }
        else
        {
            output = read_imagef(fIn, sampler, (int2)(x, y));
        }

        write_imagef(fOut, (int2)(x, y), output);
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
