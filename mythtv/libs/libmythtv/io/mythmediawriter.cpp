/*
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

// MythTV
#include "io/mythmediawriter.h"

void MythMediaWriter::SetFilename(const QString &FileName)
{
    m_filename = FileName;
}

void MythMediaWriter::SetContainer(const QString &Cont)
{
    m_container = Cont;
}

void MythMediaWriter::SetVideoCodec(const QString &Codec)
{
    m_videoCodec = Codec;
}

void MythMediaWriter::SetVideoBitrate(int Bitrate)
{
    m_videoBitrate = Bitrate;
}

void MythMediaWriter::SetWidth(int Width)
{
    m_width = Width;
}

void MythMediaWriter::SetHeight(int Height)
{
    m_height = Height;
}

void MythMediaWriter::SetAspect(float Aspect)
{
    m_aspect = Aspect;
}

void MythMediaWriter::SetFramerate(double Rate)
{
    m_frameRate = Rate;
}

void MythMediaWriter::SetKeyFrameDist(int Dist)
{
    m_keyFrameDist = Dist;
}

void MythMediaWriter::SetAudioCodec(const QString &Codec)
{
    m_audioCodec = Codec;
}

void MythMediaWriter::SetAudioBitrate(int Bitrate)
{
    m_audioBitrate = Bitrate;
}

void MythMediaWriter::SetAudioChannels(int Channels)
{
    m_audioChannels = Channels;
}

void MythMediaWriter::SetAudioFrameRate(int Rate)
{
    m_audioFrameRate = Rate;
}

void MythMediaWriter::SetAudioFormat(AudioFormat Format)
{
    m_audioFormat = Format;
}

void MythMediaWriter::SetThreadCount(int Count)
{
    m_encodingThreadCount = Count;
}

void MythMediaWriter::SetTimecodeOffset(std::chrono::milliseconds Offset)
{
    m_startingTimecodeOffset = Offset;
}

void MythMediaWriter::SetEncodingPreset(const QString &Preset)
{
    m_encodingPreset = Preset;
}

void MythMediaWriter::SetEncodingTune(const QString &Tune)
{
    m_encodingTune = Tune;
}

long long MythMediaWriter::GetFramesWritten(void) const
{
    return m_framesWritten;
}

std::chrono::milliseconds MythMediaWriter::GetTimecodeOffset(void) const
{
    return m_startingTimecodeOffset;
}

int MythMediaWriter::GetAudioFrameSize(void) const
{
    return m_audioFrameSize;
}
