/*
Copyright (C) 2007 Christian Kothe, Mark Spieth

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

#ifndef FREESURROUND_H
#define FREESURROUND_H

#include "compat.h"  // instead of sys/types.h, for MinGW compatibility

#define SURROUND_BUFSIZE 8192

class FreeSurround
{
public:
    typedef enum 
    {
        SurroundModePassive,
        SurroundModeActiveSimple,
        SurroundModeActiveLinear,
        SurroundModePassiveHall
    } SurroundMode;
public:
    FreeSurround(uint srate, bool moviemode, SurroundMode mode);
    ~FreeSurround();

    // put frames in buffer, returns number of frames used
    uint putFrames(void* buffer, uint numFrames, uint numChannels);
    // get a number of frames
    uint receiveFrames(void *buffer, uint maxFrames);
    // flush unprocessed samples
    void flush();
    uint numUnprocessedFrames();
    uint numFrames();

    long long getLatency();
    uint frameLatency();

    uint framesPerBlock();

protected:
    void process_block();
    void open();
    void close();
    void SetParams();

private:

    // the changeable parameters
    struct fsurround_params {
        int32_t center_width;           // presence of the center channel
        int32_t dimension;              // dimension
        float coeff_a,coeff_b;          // surround mixing coefficients
        int32_t phasemode;              // phase shifting mode
        int32_t steering;               // steering mode (0=simple, 1=linear)
        int32_t front_sep, rear_sep;    // front/rear stereo separation
        // (default) constructor
        fsurround_params(int32_t center_width=100, int32_t dimension=0);
    } params;

    // additional settings
    uint srate;

    // info about the current setup
    struct buffers *bufs;               // our buffers
    class fsurround_decoder *decoder;   // the surround decoder
    int in_count;                       // amount in lt,rt
    int out_count;                      // amount in output bufs
    bool processed;             // whether processing is enabled for latency calc
    int processed_size;                 // amount processed
    SurroundMode surround_mode;         // 1 of 3 surround modes supported
    int latency_frames;                 // number of frames of incurred latency
    int channels;
};

#endif
