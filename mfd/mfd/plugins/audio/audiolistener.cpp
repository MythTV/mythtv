/*
	audiolistener.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Audio listener is a small object that gets events from the libmyth
	AudioOutput object and passed them to the AudioPlugin object
	(AudioPlugin could itself be a listener, but then it would have to be a
	QObject).

*/

#include "audiolistener.h"
#include "audio.h"

AudioListener::AudioListener(AudioPlugin *owner)
{
    parent = owner;
}

void AudioListener::customEvent(QCustomEvent *e)
{
    if(parent)
    {
        if ( (int)e->type() == OutputEvent::Info )
        {
            OutputEvent *oe = (OutputEvent *)e;
            parent->swallowOutputUpdate(
                                            oe->type(),
                                            oe->elapsedSeconds(),
                                            oe->channels(),
                                            oe->bitrate(),
                                            oe->frequency()
                                       );
        }
    }
}

AudioListener::~AudioListener()
{
}

