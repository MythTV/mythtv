/*
	database.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	This is nothing to do with SQL or MYSQL A large collection of items
	(songs) and containers (playlists) is called a "database" in DAAP.
	That's what this is.

*/

#include <iostream>
using namespace std;

#include "database.h"

Database::Database(
                    int l_id, 
                    int l_persistent_id,
                    const QString& l_name,
                    int l_expected_numb_items,
                    int l_expected_numb_containers,
                    MFD *my_mfd
                  )
{
    fake_temp_bool = false;
    id = l_id;
    persistent_id = l_persistent_id;
    name = l_name;
    expected_numb_items = l_expected_numb_items;
    expected_numb_containers = l_expected_numb_containers;
    the_mfd = my_mfd;
    /*
    my_metadatacontainer = new MetadataContainer(
                                                    the_mfd, 
                                                    the_mfd->bumpMetadataContainerIdentifier(),
                                                    MCCT_audio,
                                                    MCLT_lan
                                                );
    */
}


bool Database::hasItems()
{
    if(!fake_temp_bool)
    {
        fake_temp_bool = true;
        return false;
    }
    
    return true;
    
    cout << "I'm seeing expected_numb_items as " << expected_numb_items << endl;
/*
    if(expected_numb_items < 1)
    {
        return true;
    }
    
    return false;
*/
}

Database::~Database()
{
}

