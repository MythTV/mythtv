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
}

AudioMetadata*  MfdInfo::getAudioMetadata(int collection_id, int item_id)
{
    if(mfd_content_collection)
    {
        return mfd_content_collection->getAudioItem(collection_id, item_id);    
    }
    return NULL;
}

MfdInfo::~MfdInfo()
{
}



