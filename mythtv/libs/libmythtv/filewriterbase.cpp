/*  -*- Mode: c++ -*-
 *
 *   Class FileWriterBase
 *
 *   Copyright (C) Chris Pinkham 2011
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "mythlogging.h"
#include "filewriterbase.h"

#define LOC QString("FWB(%1): ").arg(m_filename)
#define LOC_ERR QString("FWB(%1) Error: ").arg(m_filename)

FileWriterBase::FileWriterBase()
    : m_videoBitrate(800000),   m_width(0),               m_height(0),
      m_aspect(1.333333),       m_frameRate(29.97),       m_keyFrameDist(15),
      m_audioBitrate(0),        m_audioChannels(2),
      m_audioFrameRate(48000),  m_audioFormat(FORMAT_S16),m_audioFrameSize(-1),
      m_encodingThreadCount(1),
      m_framesWritten(0),
      m_startingTimecodeOffset(-1)
{
}

FileWriterBase::~FileWriterBase()
{
}

int FileWriterBase::WriteVideoFrame(VideoFrame *frame)
{
    LOG(VB_RECORD, LOG_ERR, LOC + "WriteVideoFrame(): Shouldn't be here!");

    return 1;
}

int FileWriterBase::WriteAudioFrame(unsigned char *buf, int fnum, long long &timecode)
{
    LOG(VB_RECORD, LOG_ERR, LOC + "WriteAudioFrame(): Shouldn't be here!");

    return 1;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

