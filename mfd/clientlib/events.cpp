/*
	events.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    events that get passed to mfdinterface

*/

#include "events.h"

MfdDiscoveryEvent::MfdDiscoveryEvent(
                                     bool  is_it_lost_or_found,
                                     const QString &a_name,
                                     const QString &a_hostname, 
                                     const QString &an_ip_address,
                                     int a_port
                                    )
                  :QCustomEvent(MFD_CLIENTLIB_EVENT_DISCOVERY)
{
    lost_or_found = is_it_lost_or_found;
    name = a_name;
    hostname = a_hostname;
    ip_address = an_ip_address;
    port = a_port;
}

bool MfdDiscoveryEvent::getLostOrFound()
{
    return lost_or_found;
}

QString MfdDiscoveryEvent::getName()
{
    return name;
}

QString MfdDiscoveryEvent::getHost()
{
    return hostname;
}

QString MfdDiscoveryEvent::getAddress()
{
    return ip_address;
}

int MfdDiscoveryEvent::getPort()
{
    return port;
}

/*
---------------------------------------------------------------------
*/

MfdAudioPausedEvent::MfdAudioPausedEvent(
                                            int which_mfd,
                                            bool on_or_off
                                        )
                    :QCustomEvent(MFD_CLIENTLIB_EVENT_AUDIOPAUSE)

{
    mfd_id = which_mfd;
    paused_or_not = on_or_off;
}

int MfdAudioPausedEvent::getMfd()
{
    return mfd_id;
}

bool MfdAudioPausedEvent::getPaused()
{
    return paused_or_not;
}

/*
---------------------------------------------------------------------
*/

MfdAudioStoppedEvent::MfdAudioStoppedEvent(int which_mfd)
                    :QCustomEvent(MFD_CLIENTLIB_EVENT_AUDIOSTOP)

{
    mfd_id = which_mfd;
}

int MfdAudioStoppedEvent::getMfd()
{
    return mfd_id;
}

/*
---------------------------------------------------------------------
*/

MfdAudioPlayingEvent::MfdAudioPlayingEvent(
                                            int which_mfd,
                                            int which_playlist,
                                            int index_in_playlist,
                                            int which_collection_id,
                                            int which_metadata_id,
                                            int seconds_elapsed,
                                            int numb_channels,
                                            int what_bit_rate,
                                            int what_sample_frequency
                                          )
                     :QCustomEvent(MFD_CLIENTLIB_EVENT_AUDIOPLAY)

{
    mfd_id = which_mfd;
    playlist = which_playlist;
    playlist_index = index_in_playlist;
    collection_id = which_collection_id;
    metadata_id = which_metadata_id;
    seconds = seconds_elapsed;
    channels = numb_channels;
    bit_rate = what_bit_rate;
    sample_frequency = what_sample_frequency;
}

