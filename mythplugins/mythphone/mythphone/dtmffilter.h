#ifndef DTMFFILTER_H_
#define DTMFFILTER_H_

#include <qmap.h>


class goertzel
{
  public:
    goertzel(int N, float targetFreq, float sampleRate);
    ~goertzel();
    int process(short *samples, int length);

  private:
    void checkMatch();
    void processOneSample(short sample);
    void reset();
    void initialise(int N, float targetFreq, float sampleRate);
    
    float Q1, Q2, coeff, sine, cosine;
    int countToN, m_N;
    int Match;
};


class DtmfFilter
{
  public:
    DtmfFilter();
    ~DtmfFilter();
    QChar process(short *samples, int length);
    
  private:
    void HitCounter(int Frequency, int numHitsThisSample);
    QChar CheckAnyDTMF();
    
    goertzel *filter_697;
    goertzel *filter_770;
    goertzel *filter_852;
    goertzel *filter_941;
    goertzel *filter_1209;
    goertzel *filter_1336;
    goertzel *filter_1477;
    
    QMap<int,int> Hits;
    QMap<QChar, unsigned int> Debounce;
};

#endif
