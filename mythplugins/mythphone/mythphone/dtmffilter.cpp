/*
        dtmffilter.cpp

        (c) 2005 Paul Volkaerts
        Part of the mythTV project
        
        This file contains a set of procedures to efficiently detect DTMF tones within a sample
        waveform. Most SIP applications should use RFC2833 making this unnecessary but many
        VoIP gateways have this disabled.
        
        This class uses a "Goertzel" algorithm, as described at www.embedded.com
        
*/


#include <stdio.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

#include "config.h"
#include "dtmffilter.h"



/**********************************************************************
DtmfFilter

A DTMF Filter algorithm based on the goertzel class

**********************************************************************/

DtmfFilter::DtmfFilter()
{
    // Set up the Goertzel filters to match the DTMF frequencies
    // 697, 770, 852 & 941 Hz plus
    // 1209, 1336, 1477 & Hz

    filter_697 = new goertzel(218, 697, 8000);
    filter_770 = new goertzel(187, 770, 8000);
    filter_852 = new goertzel(169, 852, 8000);
    filter_941 = new goertzel(204, 941, 8000);
    filter_1209 = new goertzel(172, 1209, 8000);
    filter_1336 = new goertzel(491, 1336, 8000);
    filter_1477 = new goertzel(260, 1477, 8000);

    Hits[697]  = 0;
    Hits[770]  = 0;
    Hits[852]  = 0;
    Hits[941]  = 0;
    Hits[1209] = 0;
    Hits[1336] = 0;
    Hits[1477] = 0;

    Debounce['0'] = 0;    
    Debounce['1'] = 0;    
    Debounce['2'] = 0;    
    Debounce['3'] = 0;    
    Debounce['4'] = 0;    
    Debounce['5'] = 0;    
    Debounce['6'] = 0;    
    Debounce['7'] = 0;    
    Debounce['8'] = 0;    
    Debounce['9'] = 0;    
    Debounce['*'] = 0;    
    Debounce['#'] = 0;    
}

DtmfFilter::~DtmfFilter()
{
    delete filter_697;
    delete filter_770;
    delete filter_852;
    delete filter_941;
    delete filter_1209;
    delete filter_1336;
    delete filter_1477;
}

QChar DtmfFilter::process(short *samples, int length)
{
    HitCounter(697, filter_697->process(samples, length));
    HitCounter(770, filter_770->process(samples, length));
    HitCounter(852, filter_852->process(samples, length));
    HitCounter(941, filter_941->process(samples, length));
    HitCounter(1209, filter_1209->process(samples, length));
    HitCounter(1336, filter_1336->process(samples, length));
    HitCounter(1477, filter_1477->process(samples, length));
    
    return CheckAnyDTMF();
}

void DtmfFilter::HitCounter(int Frequency, int numHitsThisSample)
{
    // This checks for hits on frequencies and can provide some debouncing
    if (numHitsThisSample == 0)
        Hits[Frequency] = 0;
    else 
        Hits[Frequency] += numHitsThisSample;
}

QChar DtmfFilter::CheckAnyDTMF()
{
    unsigned char Match = 0;
    QChar key=0;
    if (Hits[697]  > 0) Match |= 0x01;
    if (Hits[770]  > 0) Match |= 0x02;
    if (Hits[852]  > 0) Match |= 0x04;
    if (Hits[941]  > 0) Match |= 0x08;
    if (Hits[1209] > 0) Match |= 0x10;
    if (Hits[1336] > 0) Match |= 0x20;
    if (Hits[1477] > 0) Match |= 0x40;

    Debounce['0'] = (Debounce['0'] << 1) & 0xFF;
    Debounce['1'] = (Debounce['1'] << 1) & 0xFF;
    Debounce['2'] = (Debounce['2'] << 1) & 0xFF;
    Debounce['3'] = (Debounce['3'] << 1) & 0xFF;
    Debounce['4'] = (Debounce['4'] << 1) & 0xFF;
    Debounce['5'] = (Debounce['5'] << 1) & 0xFF;
    Debounce['6'] = (Debounce['6'] << 1) & 0xFF;
    Debounce['7'] = (Debounce['7'] << 1) & 0xFF;
    Debounce['8'] = (Debounce['8'] << 1) & 0xFF;
    Debounce['9'] = (Debounce['9'] << 1) & 0xFF;
    Debounce['*'] = (Debounce['*'] << 1) & 0xFF;
    Debounce['#'] = (Debounce['#'] << 1) & 0xFF;
        
    switch (Match)
    {
    default: // No set of frequencies which constitute a DTMF code matched
        return 0;
        break;
    case 0x11:    key = '1'; break;
    case 0x12:    key = '4'; break;
    case 0x14:    key = '7'; break;
    case 0x18:    key = '*'; break;
    case 0x21:    key = '2'; break;
    case 0x22:    key = '5'; break;
    case 0x24:    key = '8'; break;
    case 0x28:    key = '0'; break;
    case 0x41:    key = '3'; break;
    case 0x42:    key = '6'; break;
    case 0x44:    key = '9'; break;
    case 0x48:    key = '#'; break;
    }
    
    // Simple Debounce; matches on first hit and hit persists for 8 sample times
    Debounce[key] |= 0x01;
    if (Debounce[key] != 1) 
        return 0;
        
    cout << "DTMF Filter matched" << key << endl;    
    return key;
}



/**********************************************************************
goertzel

Goertzel Generic Filter class.

**********************************************************************/

goertzel::goertzel(int N, float targetFreq, float sampleRate)
{
    initialise(N, targetFreq, sampleRate);
}

goertzel::~goertzel()
{
}

void goertzel::checkMatch()
{
    float magSquared;
    float magnitude;
    
    magSquared = Q1 * Q1 + Q2 * Q2 - Q1 * Q2 * coeff;
    magnitude = sqrt(magSquared);

//cout << "Goertzel magnitude " << magnitude << endl;    
    
    if (magnitude > 500000.0) // Need to fine tune this match level as GSM codec may interfere
        Match++;
        
    reset();
}

int goertzel::process(short *samples, int length)
{
    Match = 0;
    while (length > 0)
    {
        while ((length > 0) && (countToN < m_N))
        {
            processOneSample(*samples++);
            length--;
            countToN++;
        }
        if (m_N == countToN)
            checkMatch();
    }
    return Match;
}

void goertzel::processOneSample(short sample)
{
    float Q0;
    Q0 = coeff * Q1 - Q2 + (float)sample;
    Q2 = Q1;
    Q1 = Q0;
}

// Reset needs to be called after every block is processed
void goertzel::reset()
{
    Q2=0;
    Q1=0;
    countToN = 0;
}

// Call once to initialise
void goertzel::initialise(int N, float targetFreq, float sampleRate)
{
    int k;
    float floatN;
    float Omega;
    
    m_N = N;
    floatN = (float)N;
    k = (int)(0.5 + ((floatN * targetFreq) / sampleRate));
    Omega = (2.0 * M_PI * k) / floatN;
    sine = sin(Omega);
    cosine = cos(Omega);
    coeff = 2.0 * cosine;
    
    reset();
}





