/*
	mfdinfo.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;

#include "mfdinfo.h"

MfdInfo::MfdInfo( int an_id, const QString &a_name, const QString &a_host)
{
    id = an_id;
    name = a_name;
    host = a_host;
    mfd_content_collection = NULL;
    showing_menu = false;
    played_percentage = 0;
    pause_state = false;
    knows_whats_playing = true;
    current_container = -1;
    current_item = -1;
    previous_container = -2;
    previous_item = -2;
    current_elapsed = -1;
    is_stopped = true;
    my_speakers.setAutoDelete(true);
    my_jobs.setAutoDelete(true);
    has_transcoder = false;
}

AudioMetadata*  MfdInfo::getAudioMetadata(int collection_id, int item_id)
{
    if (mfd_content_collection)
    {
        return mfd_content_collection->getAudioItem(collection_id, item_id);    
    }
    return NULL;
}

ClientPlaylist*  MfdInfo::getAudioPlaylist(int collection_id, int item_id)
{
    if (mfd_content_collection)
    {
        return mfd_content_collection->getAudioPlaylist(collection_id, item_id);    
    }
    return NULL;
}

UIListGenericTree* MfdInfo::getPlaylistTree(
                                    int collection_id, 
                                    int playlist_id,
                                    bool pristine
                                   )
{

    if (mfd_content_collection)
    {
    
        return mfd_content_collection->getPlaylistTree(
                                                        collection_id, 
                                                        playlist_id,
                                                        pristine
                                                      );
    }
    return NULL;
}

UIListGenericTree* MfdInfo::getContentTree(int collection_id, bool pristine)
{
    if (mfd_content_collection)
    {
        return mfd_content_collection->getContentTree(collection_id, pristine);
    }
    return NULL;
}

void MfdInfo::toggleItem(UIListGenericTree *node, bool turn_on)
{
    if (mfd_content_collection)
    {
        mfd_content_collection->toggleItem(node, turn_on);
    }
}

void MfdInfo::toggleTree(
                            UIListTreeType *menu, 
                            UIListGenericTree *playlist_tree, 
                            UIListGenericTree *node, 
                            bool turn_on,
                            QIntDict<bool> *playlist_additions,
                            QIntDict<bool> *playlist_deletions
                        )
{
    if (mfd_content_collection)
    {
        mfd_content_collection->toggleTree(
                                            menu, 
                                            playlist_tree, 
                                            node, 
                                            turn_on,
                                            playlist_additions,
                                            playlist_deletions
                                          );
    }
}

void MfdInfo::updatePlaylistDeltas(
                                    QIntDict<bool> *playlist_additions,
                                    QIntDict<bool> *playlist_deletions,
                                    bool addition,
                                    int item_id
                                  )
{
    if (mfd_content_collection)
    {
        mfd_content_collection->updatePlaylistDeltas(
                                                        playlist_additions, 
                                                        playlist_deletions,
                                                        addition,
                                                        item_id
                                                    );
    }
}

void MfdInfo::alterPlaylist(UIListTreeType *menu, UIListGenericTree *playlist_tree, UIListGenericTree *node, bool turn_on)
{
    if (mfd_content_collection)
    {
        mfd_content_collection->alterPlaylist(menu, playlist_tree, node, turn_on);
    }
}

void MfdInfo::setCurrentPlayingData()
{
    if (
        current_container > -1 &&
        current_item > -1 &&
        current_elapsed > -1
      )
    {
        setCurrentPlayingData(current_container, current_item, current_elapsed);
    }
}


bool MfdInfo::setCurrentPlayingData(int which_container, int which_metadata, int numb_seconds)
{

    current_container = which_container;
    current_item = which_metadata;
    current_elapsed = numb_seconds;

    AudioMetadata *whats_playing = getAudioMetadata(which_container, which_metadata);
    playing_strings.clear();
    if (whats_playing)
    {
        bool return_value = true;
        playing_strings.append(whats_playing->getTitle());
        playing_strings.append(whats_playing->getArtist());
        playing_strings.append(whats_playing->getAlbum());
        played_percentage = (double) ((numb_seconds + 0.0) * 1000.0) / (whats_playing->getLength() + 0.0);
        knows_whats_playing = true;
        
        if (previous_container == current_container && previous_item == current_item)
        {
            return_value = false;
        }
        previous_container = current_container;
        previous_item = current_item;
     
        return return_value;   
    }
    else
    {
        played_percentage = 0.0;
        knows_whats_playing = false;
        previous_container = -2;
        previous_item = -2;
    }
    return false;
}

void MfdInfo::markNodeAsHeld(UIListGenericTree *node, bool held_or_not)
{
    if (mfd_content_collection)
    {
        mfd_content_collection->markNodeAsHeld(node, held_or_not);
    }
}

int MfdInfo::countTracks(UIListGenericTree *playlist_tree)
{
    if (mfd_content_collection)
    {
        return mfd_content_collection->countTracks(playlist_tree);
    }
    return 0;
}

void MfdInfo::setSpeakerList(QPtrList<SpeakerTracker>* new_speakers)
{
    //
    //  We have a new canonical list of speakers for this mfd. Make a copy
    //  and store them.
    //   
    
    my_speakers.clear();
    
    SpeakerTracker *a_speaker;
    QPtrListIterator<SpeakerTracker> iter( *new_speakers );

    while ( (a_speaker = iter.current()) != 0 )
    {
        SpeakerTracker *new_speaker = new SpeakerTracker(a_speaker->getId(), a_speaker->getInUse());
        new_speaker->setName(a_speaker->getName());
        my_speakers.append(new_speaker);
        ++iter;
    }
    
}

void MfdInfo::setJobList(QPtrList<JobTracker>* new_jobs)
{
    //
    //  We have a new list of jobs/status for this mfd. Make a copy
    //  and store them.
    //   
    
    my_jobs.clear();
    
    JobTracker *a_job;
    QPtrListIterator<JobTracker> iter( *new_jobs );

    while ( (a_job = iter.current()) != 0 )
    {
        JobTracker *new_job = new JobTracker(*a_job);
        my_jobs.append(new_job);
        ++iter;
    }
    
}

void MfdInfo::printTree(UIListGenericTree *node, int depth)
{
    if (mfd_content_collection)
    {
        mfd_content_collection->printTree(node, depth);
    }
}

MfdInfo::~MfdInfo()
{
    my_speakers.clear();
    my_jobs.clear();
}



