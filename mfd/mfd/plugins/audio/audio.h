#ifndef AUDIO_H_
#define AUDIO_H_
/*
	audio.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the audio plugin
*/

#include <qiodevice.h>
#include <qsocketdevice.h>
#include <qurl.h>
#include <qptrlist.h>

#include "mfd_plugin.h"

#include "output.h"
#include "decoder.h"

class AudioPlugin: public MFDServicePlugin
{

  public:

    AudioPlugin(MFD *owner, int identity);
    void    run();
    void    doSomething(const QStringList &tokens, int socket_identifier);

    bool    playAudio(QUrl url);
    void    stopAudio();
    void    pauseAudio(bool true_or_false);
    void    seekAudio(int seek_amount);
    void    swallowOutputUpdate(int numb_seconds, int channels, int bitrate, int frequency);
        
  private:
  
    QIODevice   *input;
    Output      *output;
    Decoder     *decoder;
    
    int                           output_buffer_size;
    bool                          is_playing;
    bool                          is_paused;
    QString                       audio_device;

    MFDFileDescriptorWatchingPlugin *fd_watcher;
    QMutex file_descriptors_mutex;
    
    int     elapsed_time;
    int     current_channels;
    int     current_bitrate;
    int     current_frequency;
    
    QMutex play_data_mutex;
};

#endif  // audio_h_
