#ifndef AUDIOCDJOB_H_
#define AUDIOCDJOB_H_
/*
	audiocdjob.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Class for importing audio cds
*/

#include <qurl.h>

#include "importjob.h"
#include "metadata.h"

enum MfdTranscoderAudioCodec
{
    MFD_TRANSCODER_AUDIO_CODEC_OGG = 0,
    MFD_TRANSCODER_AUDIO_CODEC_FLAC,
    MFD_TRANSCODER_AUDIO_CODEC_MP3
};

enum MfdTranscoderAudioQuality
{
    MFD_TRANSCODER_AUDIO_QUALITY_LOW = 0,
    MFD_TRANSCODER_AUDIO_QUALITY_MED,
    MFD_TRANSCODER_AUDIO_QUALITY_HIGH,
    MFD_TRANSCODER_AUDIO_QUALITY_PERFECT
};

class AudioCdJob: public ImportJob
{

  public:

    AudioCdJob(
                    Transcoder                 *owner,
                    int                         l_job_id,
                    QString                     l_scratch_dir_string,
                    QString                     l_destination_dir_string,
                    MetadataServer             *l_metadata_server,
                    int                         l_container_id,
                    int                         l_playlist_id,
                    MfdTranscoderAudioCodec     l_codec,
                    MfdTranscoderAudioQuality   l_quality
             );

    virtual ~AudioCdJob();

    void run();
    
  protected:
  
    bool            ripAndEncodeTrack(QUrl track_url, QString destination_filename, AudioMetadata *metadata);
    void            tryToCleanUp(QCString filename);
    void            handleFileTokens(QString &filename, AudioMetadata *track);
    static QString  fixFileToken(QString token);
    void            ejectCd(QString dev);

    MfdTranscoderAudioCodec     codec;
    MfdTranscoderAudioQuality   quality;
};

#endif  // audiocdjob_h_
