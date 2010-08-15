#include "audiooutputdownmix.h"

#include "string.h"

/*
 SMPTE channel layout
 DUAL-MONO      L   R
 DUAL-MONO-LFE  L   R   LFE
 MONO           M
 MONO-LFE       M   LFE
 STEREO         L   R
 STEREO-LFE     L   R   LFE
 3F             L   R   C
 3F-LFE         L   R   C    LFE
 2F1            L   R   S
 2F1-LFE        L   R   LFE  S
 3F1            L   R   C    S
 3F1-LFE        L   R   C    LFE S
 2F2            L   R   LS   RS
 2F2-LFE        L   R   LFE  LS   RS
 3F2            L   R   C    LS   RS
 3F2-LFE        L   R   C    LFE  LS   RS
 3F4            L   R   C    Rls  Rrs  LS   RS
 3F4-LFE        L   R   C    LFE  Rls  Rrs  LS   RS
 */

static const float m3db = 0.7071067811865476f;           // 3dB  = SQRT(2)
static const float mm3db = -0.7071067811865476f;         // -3dB = SQRT(1/2)
static const float msqrt_1_3 = -0.577350269189626f;      // -SQRT(1/3)
static const float sqrt_2_3 = 0.816496580927726f;        // SQRT(2/3)
static const float sqrt_2_3by3db = 0.577350269189626f;   // SQRT(2/3)*-3dB = SQRT(2/3)*SQRT(1/2)=SQRT(1/3)
static const float msqrt_1_3bym3db = 0.408248290463863f; // -SQRT(1/3)*-3dB = -SQRT(1/3)*SQRT(1/2) = -SQRT(1/6)

int matrix[4][2] = { { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 } };

static const float stereo_matrix[8][8][2] =
{
//1F      L                R
    {
        { 1,               1 },                 // M
    },

//2F      L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
    },

//3F      L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { 1,               1 },                 // C
    },

//3F1R    L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { mm3db,           m3db },              // S
    },

//3F2R    L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { sqrt_2_3,        msqrt_1_3 },         // LS
        { msqrt_1_3,       sqrt_2_3 },          // RS
    },

//3F2R.1  L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { 0,               0 },                 // LFE
        { sqrt_2_3,        msqrt_1_3 },         // LS
        { msqrt_1_3,       sqrt_2_3 },          // RS
    },

// 3F4R   L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { sqrt_2_3by3db,   msqrt_1_3bym3db },   // Rls
        { msqrt_1_3bym3db, sqrt_2_3by3db },     // Rrs
        { sqrt_2_3by3db,   msqrt_1_3bym3db },   // LS
        { msqrt_1_3bym3db, sqrt_2_3by3db },     // RS
    },

// 3F4R.1 L                R
    {
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { 0,               0 },                 // LFE
        { sqrt_2_3by3db,   msqrt_1_3bym3db },   // Rls
        { msqrt_1_3bym3db, sqrt_2_3by3db },     // Rrs
        { sqrt_2_3by3db,   msqrt_1_3bym3db },   // LS
        { msqrt_1_3bym3db, sqrt_2_3by3db },     // RS
    }
};

static const float s51_matrix[2][8][6] =
{ 
    // 3F4R in -> 3F2R.1 out
    // L  R  C  LFE      LS       RS
    {
        { 1, 0, 0, 0,       0,       0 },     // L
        { 0, 1, 0, 0,       0,       0 },     // R
        { 0, 0, 1, 0,       0,       0 },     // C
        { 0, 0, 0, 0,       m3db,    0 },     // Rls
        { 0, 0, 0, 0,       0,       m3db },  // Rrs
        { 0, 0, 0, 0,       m3db,    0 },     // LS
        { 0, 0, 0, 0,       0,       m3db },  // RS
    },
    // 3F4R.1 -> 3F2R.1 out
    // L  R  C  LFE      LS       RS
    {
        { 1, 0, 0, 0,       0,       0 },     // L
        { 0, 1, 0, 0,       0,       0 },     // R
        { 0, 0, 1, 0,       0,       0 },     // C
        { 0, 0, 0, 1,       0,       1 },     // LFE
        { 0, 0, 0, 0,       m3db,    0 },     // Rls
        { 0, 0, 0, 0,       0,       m3db },  // Rrs
        { 0, 0, 0, 0,       m3db,    0 },     // LS
        { 0, 0, 0, 0,       0,       m3db },  // RS
    }
};

int AudioOutputDownmix::DownmixFrames(int channels_in, int  channels_out,
                                      float *dst, float *src, int frames)
{
    if (channels_in <= channels_out)
        return -1;

    if (channels_out == 2)
    {
        float tmp;
        int index = channels_in - 1;
        for (int n=0; n < frames; n++)
        {
            for (int i=0; i < channels_out; i++)
            {
                tmp = 0.0f;
                for (int j=0; j < channels_in; j++)
                    tmp += src[j] * stereo_matrix[index][j][i];
                *dst++ = tmp;
            }
            src += channels_in;
        }
    }
    else if (channels_out == 6)
    {
        int lensamples = channels_in - 4;
        int lenbytes = lensamples * sizeof(float);
        for (int n=0; n < frames; n++)
        {
            memcpy(dst, src, lenbytes);
            src += lensamples;
            dst += 4;
            //read value first, as src and dst can overlap
            float ls = src[0];
            float rs = src[1];
            float rls = src[2];
            float rrs = src[3];
            *dst++ = ls * m3db + rls * m3db;    // LS = LS*-3dB + Rls*-3dB
            *dst++ = rs * m3db + rrs * m3db;    // RS = RS*-3dB + Rrs*-3dB
            src += 4;
        }
    }
    else
        return -1;

    return frames;
}
