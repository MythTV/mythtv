/*
	wavfile.cpp

	(c) 2004 Paul Volkaerts
	
  A good description of the format can be found at 
      http://ccrma-www.stanford.edu/courses/422/projects/WaveFormat/
*/

#include <qapplication.h>
#include <qfile.h>

#include <stdlib.h>
#include <stdio.h>
#include <iostream>

using namespace std;

#include "wavfile.h"
#include <mythtv/mythverbose.h>

wavfile::wavfile()
{
    loaded=false;
    audio = 0;
}

wavfile::~wavfile()
{
    if (loaded && audio)
        delete audio;
}

bool wavfile::load(const char *Filename)
{
    QFile f(Filename);
    if (!f.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("Cannot open for reading file %1").arg(Filename));
        return false;
    }

    w.ChunkSize = 36 + w.subChunk2Size;
    int result = f.readBlock((char *)&w, sizeof(w));
    if (result == -1)
    {
        f.close();
        return false;
    }

    audio = new char[w.subChunk2Size];
    result = f.readBlock(audio, w.subChunk2Size);
    loaded=true;
    f.close();

    return (result == -1 ? false : true);
}

void wavfile::load(short *data, int samples, int bitsPerSample, int AudioFormat, int nChan, int SampleRate)
{
    memcpy(w.ChunkId, "RIFF", 4);
    memcpy(w.Format, "WAVE", 4);
    memcpy(w.subChunk1Id, "fmt ", 4);
    w.subChunk1Size = 16;
    w.AudioFormat = AudioFormat; 
    w.NumChannels = nChan;
    w.SampleRate = SampleRate;
    w.ByteRate = w.SampleRate * w.NumChannels * (bitsPerSample/8);
    w.BlockAlign = w.NumChannels * (bitsPerSample/8);
    w.BitsPerSample = bitsPerSample;
    memcpy(w.subChunk2Id, "data", 4);
    w.subChunk2Size = samples*(bitsPerSample/8);

    if (audio)
    {
        delete audio;
        audio = 0;
    }

    audio = new char[w.subChunk2Size];
    memcpy(audio, (const char *)data, w.subChunk2Size);

    if (w.SampleRate != 8000)
        transcodeTo8K();
 
    loaded=true;
}

void wavfile::transcodeTo8K()
{
static bool firstTime = true;

    // The TTS library by default uses 16k PCM. It is better to reconfigure it to 8K PCM
    // but just to be forgiving, perform simple transcodes here & warn the user
    if (audio)
    {
        if (w.SampleRate == 16000)
        {
            short *s = (short *)audio;
            short *d = (short *)audio;
            w.subChunk2Size /= 2;
            for (uint c=0; c<w.subChunk2Size/sizeof(short); c++,s++)
                *d++ = *s++;
            w.SampleRate = 8000;
            w.ByteRate = w.SampleRate * w.NumChannels * (w.BitsPerSample/8);
 
            if (firstTime)
            {
                firstTime = false;
                VERBOSE(VB_IMPORTANT, "The TTS library is encoding as 16k PCM, you should reconfigure it to 8k PCM");
            }
        }
        else
            VERBOSE(VB_IMPORTANT, QString("MythPhone Unsupported sample-rate %1").arg(w.SampleRate));
    }
}

void wavfile::print()
{
    if (loaded)
    {
        if (memcmp(w.ChunkId, "RIFF", 4) == 0)
        {
            VERBOSE(VB_IMPORTANT, "Filetype: RIFF");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Filetype: Unsupported");
        }

        if (memcmp(w.Format, "WAVE", 4) == 0)
        {
            VERBOSE(VB_IMPORTANT, "Format: WAVE");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Format: Unsupported");
        }

        if (memcmp(w.subChunk1Id, "fmt ", 4) == 0)
        {
            VERBOSE(VB_IMPORTANT, "SubFormat: fmt");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("SubFormat: Unsupported\n"
                            "ChunkSize: %1\n"
                            "Audio Format: %2\n"
                            "Channels: %3\n"
                            "Sample Rate: %4\n"
                            "Byte Rate: %5\n"
                            "Block Align: %6\n"
                            "Bits per Sample: %7")
                            .arg(w.subChunk1Size)
                            .arg(w.AudioFormat==1 ? "PCM" : "Unsupported")
                            .arg(w.NumChannels)
                            .arg(w.SampleRate)
                            .arg(w.ByteRate)
                            .arg(w.BlockAlign)
                            .arg(w.BitsPerSample));
        }

        if (memcmp(w.subChunk2Id, "data", 4) == 0)
        {
            VERBOSE(VB_IMPORTANT, "SubFormat: data");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "SubFormat: Unsupported");
        }

        VERBOSE(VB_IMPORTANT, QString("DataSize: %1").arg(w.subChunk2Size));
    }
}

bool wavfile::saveToFile(const char *Filename)
{
    QFile f(Filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, 
                QString("Cannot open for writing file %1").arg(Filename));
        return false;
    }

    w.ChunkSize = 36 + w.subChunk2Size;
    int result = f.writeBlock((const char *)&w, sizeof(w));
    if ((result != -1) && audio)
        result = f.writeBlock(audio, w.subChunk2Size);
    f.close();

    return (result == -1 ? false : true);
}















