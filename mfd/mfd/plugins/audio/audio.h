#ifndef AUDIO_H_
#define AUDIO_H_
/*
	audio.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the audio plugin
*/

#include <qiodevice.h>
#include <qsocketdevice.h>
#include <qurl.h>
#include <qptrlist.h>
#include <qvaluestack.h>

#include <mythtv/audiooutput.h>

#include "mfd_plugin.h"
#include "../../mdserver.h"

#include "decoder.h"
#include "maopinstance.h"

class AudioListener;

#ifdef MFD_RTSP_SUPPORT
class RtspOut;
#endif

class AudioPlugin: public MFDServicePlugin
{

  public:

    AudioPlugin(MFD *owner, int identity);
    ~AudioPlugin();
    void    run();
    void    doSomething(const QStringList &tokens, int socket_identifier);

    bool    playUrl(QUrl url, int collection_id = -1);
    bool    playMetadata(int collection_id, int metadata_id);
    void    stopAudio();
    void    pauseAudio(bool true_or_false);
    void    seekAudio(int seek_amount);
    void    swallowOutputUpdate(int type, int numb_seconds, int channels, int bitrate, int frequency);
    void    handleInternalMessage(QString the_message);
    void    setPlaylistMode(int container, int id, int index = 0);
    void    stopPlaylistMode();
    void    playFromPlaylist(int augment_index = 0);
    void    nextPrevAudio(bool forward);
    void    handleMetadataChange(int which_collection, bool external=false);
    void    deleteOutput();        
    void    handleServiceChange();
    void    addMaopSpeakers(QString l_address, uint l_port, QString l_name, ServiceLocationDescription l_location, bool *something_changed);
    void    checkSpeakers();
    void    turnOnSpeakers();
    void    turnOffSpeakers();
    void    listSpeakers();
    void    nameSpeakers(int which_ont, int socket_identifier);
    void    useSpeakers(int which_one, bool turn_on);
    int     bumpSpeakerId(){ unique_speaker_id++; return unique_speaker_id; }
  private:
  
    QIODevice   *input;
    AudioOutput *output;
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
    int     current_playlist;
    int     current_playlist_item;
    QMutex  play_data_mutex;

    QValueStack<int>    current_chain_playlist;
    QValueStack<int>    current_chain_position;

    MetadataServer *metadata_server;
    
    QMutex  playlist_mode_mutex;
    bool    playlist_mode;
    int     current_playlist_container;
    int     current_playlist_id;
    int     current_playlist_item_index;

    AudioListener *audio_listener;
#ifdef MFD_RTSP_SUPPORT
    RtspOut       *rtsp_out;
#endif

    QMutex                 maop_mutex;
    QPtrList<MaopInstance> maop_instances;
    bool                   waiting_for_speaker_release;
    QTime                  speaker_release_timer;
    int                    unique_speaker_id;
};

#endif  // audio_h_
