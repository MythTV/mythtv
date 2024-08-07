/*
Copyright (C) 2010-2011 Jean-Yves Avenard

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "audiooutputbase.h"
#include "audiooutputdownmix.h"

#include <cstring>

#define LOC QString("Downmixer: ")

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
 3F3R-LFE       L   R   C    LFE  BC   LS   RS
 3F4-LFE        L   R   C    LFE  Rls  Rrs  LS   RS
 */

static const float m6db = 0.5;
static const float m3db = 0.7071067811865476F;           // 3dB  = SQRT(2)
static const float mm3db = -0.7071067811865476F;         // -3dB = SQRT(1/2)
static const float msqrt_1_3 = -0.577350269189626F;      // -SQRT(1/3)
static const float sqrt_2_3 = 0.816496580927726F;        // SQRT(2/3)
static const float sqrt_2_3by3db = 0.577350269189626F;   // SQRT(2/3)*-3dB = SQRT(2/3)*SQRT(1/2)=SQRT(1/3)
static const float msqrt_1_3bym3db = 0.408248290463863F; // -SQRT(1/3)*-3dB = -SQRT(1/3)*SQRT(1/2) = -SQRT(1/6)

using two_speaker_ratio = std::array<float,2>;
using two_speaker_set   = std::array<two_speaker_ratio,8>;
static const std::array<two_speaker_set,8> stereo_matrix
{{
//1F      L                R
    {{
        { 1,               1 },                 // M
    }},

//2F      L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
    }},

//3F      L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { 1,               1 },                 // C
    }},

//3F1R    L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { mm3db,           m3db },              // S
    }},

//3F2R    L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { sqrt_2_3,        msqrt_1_3 },         // LS
        { msqrt_1_3,       sqrt_2_3 },          // RS
    }},

//3F2R.1  L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { 0,               0 },                 // LFE
        { sqrt_2_3,        msqrt_1_3 },         // LS
        { msqrt_1_3,       sqrt_2_3 },          // RS
    }},

// 3F3R.1 L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { 0,               0 },                 // LFE
        { m6db,            m6db },              // Cs
        { sqrt_2_3,        msqrt_1_3 },         // LS
        { msqrt_1_3,       sqrt_2_3 },          // RS
    }},

// 3F4R.1 L                R
    {{
        { 1,               0 },                 // L
        { 0,               1 },                 // R
        { m3db,            m3db },              // C
        { 0,               0 },                 // LFE
        { sqrt_2_3by3db,   msqrt_1_3bym3db },   // Rls
        { msqrt_1_3bym3db, sqrt_2_3by3db },     // Rrs
        { sqrt_2_3by3db,   msqrt_1_3bym3db },   // LS
        { msqrt_1_3bym3db, sqrt_2_3by3db },     // RS
    }}
}};

using six_speaker_ratio = std::array<float,6>;
using six_speaker_set   = std::array<six_speaker_ratio,8>;
static const std::array<six_speaker_set,3> s51_matrix
{{
    // 3F2R.1 in -> 3F2R.1 out
    // L  R  C  LFE         LS       RS
    {{
        { 1, 0, 0, 0,       0,       0 },     // L
        { 0, 1, 0, 0,       0,       0 },     // R
        { 0, 0, 1, 0,       0,       0 },     // C
        { 0, 0, 0, 1,       0,       0 },     // LFE
        { 0, 0, 0, 0,       1,       0 },     // LS
        { 0, 0, 0, 0,       0,       1 },     // RS
    }},
    // 3F3R.1 in -> 3F2R.1 out
    // Used coefficient found at http://www.yamahaproaudio.com/training/self_training/data/smqr_en.pdf
    // L  R  C  LFE         LS       RS
    {{
        { 1, 0, 0, 0,       0,       0 },     // L
        { 0, 1, 0, 0,       0,       0 },     // R
        { 0, 0, 1, 0,       0,       0 },     // C
        { 0, 0, 0, 1,       0,       0 },     // LFE
        { 0, 0, 0, 0,       m3db,    m3db },  // Cs
        { 0, 0, 0, 0,       1,       0 },     // LS
        { 0, 0, 0, 0,       0,       1 },     // RS
    }},
    // 3F4R.1 -> 3F2R.1 out
    // L  R  C  LFE         LS       RS
    {{
        { 1, 0, 0, 0,       0,       0 },     // L
        { 0, 1, 0, 0,       0,       0 },     // R
        { 0, 0, 1, 0,       0,       0 },     // C
        { 0, 0, 0, 1,       0,       0 },     // LFE
        { 0, 0, 0, 0,       m3db,    0 },     // Rls
        { 0, 0, 0, 0,       0,       m3db },  // Rrs
        { 0, 0, 0, 0,       m3db,    0 },     // LS
        { 0, 0, 0, 0,       0,       m3db },  // RS
    }}
}};

int AudioOutputDownmix::DownmixFrames(int channels_in, int  channels_out,
                                      float *dst, const float *src, int frames)
{
    if (channels_in < channels_out)
        return -1;

    //VBAUDIO(LOC + QString("Downmixing %1 frames (in:%2 out:%3)")
    //    .arg(frames).arg(channels_in).arg(channels_out));
    if (channels_out == 2)
    {
        int index = channels_in - 1;
        for (int n=0; n < frames; n++)
        {
            for (int i=0; i < channels_out; i++)
            {
                float tmp = 0.0F;
                for (int j=0; j < channels_in; j++)
                    tmp += src[j] * stereo_matrix[index][j][i];
                *dst++ = tmp;
            }
            src += channels_in;
        }
    }
    else if (channels_out == 6)
    {
        int index = channels_in - 6;
        for (int n=0; n < frames; n++)
        {
            for (int i=0; i < channels_out; i++)
            {
                float tmp = 0.0F;
                for (int j=0; j < channels_in; j++)
                    tmp += src[j] * s51_matrix[index][j][i];
                *dst++ = tmp;
            }
            src += channels_in;
        }
    }
    else
    {
        return -1;
    }

    return frames;
}
