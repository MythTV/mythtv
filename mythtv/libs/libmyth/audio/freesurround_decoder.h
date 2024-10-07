/*
Copyright (C) 2007 Christian Kothe

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

#ifndef FREESURROUND_DECODER_H
#define FREESURROUND_DECODER_H

// the Free Surround decoder
class fsurround_decoder {
public:
    // create an instance of the decoder
    //  blocksize is fixed over the lifetime of this object for performance reasons
    explicit fsurround_decoder(unsigned blocksize=8192);
    // destructor
    ~fsurround_decoder();
    
    float ** getInputBuffers();
    float ** getOutputBuffers();

    // decode a chunk of stereo sound, has to contain exactly blocksize samples
    //  center_width [0..1] distributes the center information towards the front left/right channels, 1=full distribution, 0=no distribution
    //  dimension [0..1] moves the soundfield backwards, 0=front, 1=side
    //  adaption_rate [0..1] determines how fast the steering gets adapted, 1=instantaneous, 0.1 = very slow adaption
    //void decode(float *input[2], float *output[6], float center_width=1, float dimension=0, float adaption_rate=1);   
    void decode(float center_width=1, float dimension=0, float adaption_rate=1);
    
    // flush the internal buffers
    void flush();

    // --- advanced configuration ---

    // override the surround coefficients
    //  a is the coefficient of left rear in left total, b is the coefficient of left rear in right total; the same is true for right.
    void surround_coefficients(float a, float b);

    // set the phase shifting mode for decoding
    // 0 = (+0°,+0°)   - music mode
    // 1 = (+0°,+180°) - PowerDVD compatibility
    // 2 = (+180°,+0°) - BeSweet compatibility
    // 3 = (-90°,+90°) - This seems to work. I just don't know why.
    void phase_mode(unsigned mode);

    // override the steering mode
    //  false = simple non-linear steering (old)
    //  true  = advanced linear steering (new)
    void steering_mode(bool mode);

    // set front/rear stereo separation
    //  1.0 is default, 0.0 is mono
    void separation(float front,float rear);

    // set samplerate for lfe filter
    void sample_rate(unsigned int samplerate);

private:
    class Impl;
    Impl *m_impl; // private implementation (details hidden)
};


#endif
