/*
	tts.cpp

	(c) 2004 Paul Volkaerts
	Part of the mythTV project
*/	


// Festival speech synthesis
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

#include "config.h"
#ifdef FESTIVAL_SUPPORT
#include "festival.h"
#endif
#include "wavfile.h"
#include "tts.h"



/**********************************************************************
tts

Text To Speech interface class; seperated out to get rid of compile 
errors with clashes between the Festival class and the QT library.

**********************************************************************/

static bool ttsInitialised=false;

tts::tts()
{
#ifdef FESTIVAL_SUPPORT
    if (!ttsInitialised)
        festival_initialize(true, 210000);
    ttsInitialised = true;
#endif
}


tts::~tts()
{
#ifdef FESTIVAL_SUPPORT
    festival_tidy_up();
#endif
}


void tts::setVoice(const char *voice)
{
#ifdef FESTIVAL_SUPPORT
    char voiceCmd[100];
    if (strlen(voice) < (sizeof(voiceCmd) - 3))
    {
        sprintf(voiceCmd, "(%s)", voice);
        festival_eval_command(voiceCmd);
    }
    else
        cerr << "Voice too long" << voice << endl;
#endif
}


void tts::say(const char *sentence)
{
#ifdef FESTIVAL_SUPPORT
    festival_say_text(sentence);
#endif
}


void tts::toWavFile(const char *sentence, wavfile &outWave)
{
#ifdef FESTIVAL_SUPPORT
    EST_Wave Wave;
    if (!festival_text_to_wave(sentence, Wave))
        cout << "Festival TTS failed ro generate speech for: " << sentence << endl;

    outWave.load(Wave.values().memory(),
                 Wave.num_samples(),
                 16, 1,  // Always uses 16 bits/sample and PCM(1)
                 Wave.num_channels(),
                 Wave.sample_rate());
#endif
}



