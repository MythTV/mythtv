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
#include "../../mdserver.h"

#include "output.h"
#include "decoder.h"

class AudioPlugin: public MFDServicePlugin
{

  public:

    AudioPlugin(MFD *owner, int identity);
    ~AudioPlugin();
    void    run();
    void    doSomething(const QStringList &tokens, int socket_identifier);

    bool    playUrl(QUrl url);
    bool    playMetadata(int collection_id, int metadata_id);
    void    stopAudio();
    void    pauseAudio(bool true_or_false);
    void    seekAudio(int seek_amount);
    void    swallowOutputUpdate(int numb_seconds, int channels, int bitrate, int frequency);
    void    handleInternalMessage(QString the_message);
    void    setPlaylistMode(int container, int id, int index = 0);
    void    stopPlaylistMode();
    void    playFromPlaylist(int augment_index = 0);
    void    nextPrevAudio(bool forward);
        
  private:
  
    QString     audio_device;

    QIODevice   *input;
    Output      *output;
    Decoder     *decoder;
    bool        is_playing;
    bool        is_paused;
    QMutex      *state_of_play_mutex;

    int         output_buffer_size;

    int     elapsed_time;
    int     current_channels;
    int     current_bitrate;
    int     current_frequency;
    int     current_collection;
    int     current_metadata;
    QMutex  play_data_mutex;


    MetadataServer *metadata_server;
    
    QMutex  playlist_mode_mutex;
    bool    playlist_mode;
    int     current_playlist_container;
    int     current_playlist_id;
    int     current_playlist_item_index;

};

#endif  // audio_h_
