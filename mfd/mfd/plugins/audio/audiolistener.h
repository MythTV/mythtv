#ifndef AUDIOLISTENER_H_
#define AUDIOLISTENER_H_
/*
	audiolistener.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Audio listener is a small object that gets events from the libmyth
	AudioOutput object and passed them to the AudioPlugin object
	(AudioPlugin could itself be a listener, but then it would have to be a
	QObject).

*/

#include <qobject.h>

class AudioPlugin;

class AudioListener: public QObject
{

  public:

    AudioListener(AudioPlugin *owner);
    ~AudioListener();
    
    void customEvent(QCustomEvent *e);
    
  private:
  
    AudioPlugin *parent;
};

#endif  // audiolistener_h_
