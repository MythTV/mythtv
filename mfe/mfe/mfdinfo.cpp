/*
	mfdinfo.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include "mfdinfo.h"

MfdInfo::MfdInfo( int an_id, const QString &a_name, const QString &a_host)
{
    id = an_id;
    name = a_name;
    host = a_host;
    tree_data = NULL;
}

MfdInfo::~MfdInfo()
{
}



