__kernel
void videoInvert(__read_only image2d_t frameIn,
                 __write_only image2d_t frameOut)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                              CLK_ADDRESS_CLAMP_TO_EDGE |
                              CLK_FILTER_NEAREST;

    float4 val;
    val = read_imagef(frameIn, sampler, (int2)(x, y));
    val.x *= -1.0;
    write_imagef(frameOut, (int2)(x, y), val);
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4:filetype=c
 */
