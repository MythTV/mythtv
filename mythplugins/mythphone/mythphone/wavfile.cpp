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
        cerr << "Cannot open for reading file " << Filename << endl;
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
                cout << "The TTS library is encoding as 16k PCM, you should reconfigure it to 8k PCM\n";
            }
        }
        else
            cout << "MythPhone Unsupported sample-rate " << w.SampleRate << endl;
    }
}

void wavfile::print()
{
    if (loaded)
    {
        if (memcmp(w.ChunkId, "RIFF", 4) == 0)
            cout << "Filetype: RIFF\n";
        else
            cout << "Filetype: Unsupported\n";

        if (memcmp(w.Format, "WAVE", 4) == 0)
            cout << "Format: WAVE\n";
        else
            cout << "Format: Unsupported\n";

        if (memcmp(w.subChunk1Id, "fmt ", 4) == 0)
            cout << "SubFormat: fmt\n";
        else
            cout << "SubFormat: Unsupported\n";

        cout << "ChunkSize: " << w.subChunk1Size << endl;
        cout << "Audio Format: " << (w.AudioFormat==1 ? "PCM" : "Unsupported") << endl;
        cout << "Channels: " << w.NumChannels << endl;
        cout << "Sample Rate: " << w.SampleRate << endl;
        cout << "Byte Rate: " << w.ByteRate << endl;
        cout << "Block Align: " << w.BlockAlign << endl;
        cout << "Bits per Sample: " << w.BitsPerSample << endl;

        if (memcmp(w.subChunk2Id, "data", 4) == 0)
            cout << "SubFormat: data\n";
        else
            cout << "SubFormat: Unsupported\n";

        cout << "DataSize: " << w.subChunk2Size << endl;
    }
}

bool wavfile::saveToFile(const char *Filename)
{
    QFile f(Filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        cerr << "Cannot open for writing file " << Filename << endl;
        return false;
    }

    w.ChunkSize = 36 + w.subChunk2Size;
    int result = f.writeBlock((const char *)&w, sizeof(w));
    if ((result != -1) && audio)
        result = f.writeBlock(audio, w.subChunk2Size);
    f.close();

    return (result == -1 ? false : true);
}















