
/*
 *  Class AudioConvert
 *  Created by Jean-Yves Avenard on 10/06/13.
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef __MythXCode__audioconvert__
#define __MythXCode__audioconvert__

#include "mythexp.h"
#include "audiooutputsettings.h"

class AudioConvertInternal;

class MPUBLIC AudioConvert
{
public:

    AudioConvert(AudioFormat in, AudioFormat out);
    virtual ~AudioConvert();
    /**
     * Process
     * Parameters:
     * out  : destination buffer where converted samples will be copied
     * in   : source buffer
     * bytes: size in bytes of source buffer
     *
     * Return Value: size in bytes of samples converted or <= 0 if error
     */
    int Process(void* out, const void* in, int bytes, bool noclip = false);
    AudioFormat Out(void) { return m_out; }
    AudioFormat In(void)  { return m_in;  }

    bool operator==(AudioConvert& rhs) const
    { return m_in == rhs.m_in && m_out == rhs.m_out; }
    bool operator!=(AudioConvert& rhs) const
    { return m_in != m_out; }

    void DeinterleaveSamples(int channels,
                             uint8_t* output, const uint8_t* input,
                             int data_size);
    void InterleaveSamples(int channels,
                           uint8_t* output, const uint8_t*  const* input,
                           int data_size);
    void InterleaveSamples(int channels,
                           uint8_t* output, const uint8_t* input,
                           int data_size);

    // static utilities
    static int  toFloat(AudioFormat format, void* out, const void* in, int bytes);
    static int  fromFloat(AudioFormat format, void* out, const void* in, int bytes);
    static void MonoToStereo(void* dst, const void* src, int samples);
    static void DeinterleaveSamples(AudioFormat format, int channels,
                                    uint8_t* output, const uint8_t* input,
                                    int data_size);
    static void InterleaveSamples(AudioFormat format, int channels,
                                  uint8_t* output, const uint8_t*  const* input,
                                  int data_size);
    static void InterleaveSamples(AudioFormat format, int channels,
                                  uint8_t* output, const uint8_t* input,
                                  int data_size);
private:
    AudioConvertInternal* m_ctx;
    AudioFormat m_in, m_out;
};

#endif /* defined(__MythXCode__audioconvert__) */
