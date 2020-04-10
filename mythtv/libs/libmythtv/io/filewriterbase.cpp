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
#include "io/filewriterbase.h"

void FileWriterBase::SetFilename(const QString &FileName)
{
    m_filename = FileName;
}

void FileWriterBase::SetContainer(const QString &Cont)
{
    m_container = Cont;
}

void FileWriterBase::SetVideoCodec(const QString &Codec)
{
    m_videoCodec = Codec;
}

void FileWriterBase::SetVideoBitrate(int Bitrate)
{
    m_videoBitrate = Bitrate;
}

void FileWriterBase::SetWidth(int Width)
{
    m_width = Width;
}

void FileWriterBase::SetHeight(int Height)
{
    m_height = Height;
}

void FileWriterBase::SetAspect(float Aspect)
{
    m_aspect = Aspect;
}

void FileWriterBase::SetFramerate(double Rate)
{
    m_frameRate = Rate;
}

void FileWriterBase::SetKeyFrameDist(int Dist)
{
    m_keyFrameDist = Dist;
}

void FileWriterBase::SetAudioCodec(const QString &Codec)
{
    m_audioCodec = Codec;
}

void FileWriterBase::SetAudioBitrate(int Bitrate)
{
    m_audioBitrate = Bitrate;
}

void FileWriterBase::SetAudioChannels(int Channels)
{
    m_audioChannels = Channels;
}

void FileWriterBase::SetAudioFrameRate(int Rate)
{
    m_audioFrameRate = Rate;
}

void FileWriterBase::SetAudioFormat(AudioFormat Format)
{
    m_audioFormat = Format;
}

void FileWriterBase::SetThreadCount(int Count)
{
    m_encodingThreadCount = Count;
}

void FileWriterBase::SetTimecodeOffset(long long Offset)
{
    m_startingTimecodeOffset = Offset;
}

void FileWriterBase::SetEncodingPreset(const QString &Preset)
{
    m_encodingPreset = Preset;
}

void FileWriterBase::SetEncodingTune(const QString &Tune)
{
    m_encodingTune = Tune;
}

long long FileWriterBase::GetFramesWritten(void) const
{
    return m_framesWritten;
}

long long FileWriterBase::GetTimecodeOffset(void) const
{
    return m_startingTimecodeOffset;
}

int FileWriterBase::GetAudioFrameSize(void) const
{
    return m_audioFrameSize;
}
