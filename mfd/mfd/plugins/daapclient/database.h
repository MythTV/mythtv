#ifndef DATABASE_H_
#define DATABASE_H_
/*
	database.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	This is nothing to do with SQL or MYSQL A large collection of items
	(songs) and containers (playlists) is called a "database" in DAAP.
	That's what this is.

*/

#include <qstring.h>

#include "../../mfd.h"
#include "../../mdcontainer.h"

class Database
{

  public:
  
    Database(
                int l_id, 
                int l_persistent_id,
                const QString& l_name,
                int l_expected_numb_items,
                int l_expected_numb_containers,
                MFD *my_mfd
            );
    ~Database();

    bool    hasItems();
    int     getId(){return id;}
     
  private:   
  
    int     id;
    int     persistent_id;
    QString name;
    int     expected_numb_items;
    int     expected_numb_containers;
    
    MFD                 *the_mfd;
    MetadataContainer   *my_metadatacontainer;


    bool    fake_temp_bool;
};
#endif  // database_h_
