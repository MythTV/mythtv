/*
	wavfile.h

	(c) 2004 Paul Volkaerts
	
*/


#ifndef WAVFILE_H_
#define WAVFILE_H_



struct wavstruct
{
    char ChunkId[4];
    long ChunkSize;
    char Format[4];

    char subChunk1Id[4];
    long subChunk1Size;
    short AudioFormat;
    short NumChannels;
    long  SampleRate;
    long ByteRate;
    short BlockAlign;
    short BitsPerSample;
    
    char subChunk2Id[4];
    long subChunk2Size;
};

class wavfile
{
  public:
    wavfile();
    ~wavfile();
    bool load(const char *Filename);
    void load(short *data, int samples, int bitsPerSample=16, int AudioFormat=1, int nChan=1, int SampleRate=8000);
    void print();
    bool saveToFile(const char *Filename);
    short *getData() { return (short *)audio; };
    int samples()    { if (loaded) return (w.subChunk2Size / (w.BitsPerSample/8)); else return 0; }

  private:
    void transcodeTo8K();

    bool loaded;
    wavstruct w;
    char *audio;
};


#endif
