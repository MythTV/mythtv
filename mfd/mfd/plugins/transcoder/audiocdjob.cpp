/*
	audiocdjob.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Class for importing audio cds
*/

#include "audiocdjob.h"

AudioCdJob::AudioCdJob(
                        Transcoder                 *owner,
                        int                         l_job_id,
                        QString                     l_scratch_dir_string,
                        QString                     l_destination_dir_string,
                        MetadataServer             *l_metadata_server,
                        int                         l_container_id,
                        MfdTranscoderAudioCodec     l_codec,
                        MfdTranscoderAudioQuality   l_quality
                      )
           :ImportJob(
                        owner, 
                        l_job_id, 
                        l_scratch_dir_string, 
                        l_destination_dir_string,
                        l_metadata_server,
                        l_container_id
                     )
{
    codec = l_codec;
    quality = l_quality;
}         

void AudioCdJob::run()
{
    cout << endl;
    cout << "if the audio cd job object did anything, it would now begin encoding:" << endl;
    cout << "scratch dir: " << scratch_dir_string << endl;
    cout << "destination: " << destination_dir_string << endl << endl;
}

AudioCdJob::~AudioCdJob()
{
}
