#ifndef MFDINFO_H_
#define MFDINFO_H_
/*
	mfdinfo.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qstring.h>
#include <qstringlist.h>

#include <mfdclient/mfdcontent.h>
#include <mfdclient/metadata.h>

class MfdInfo
{

  public:

    MfdInfo(int an_id, const QString &a_name, const QString &a_host);
    ~MfdInfo();

    int                     getId(){ return id; }

    void                    setMfdContentCollection(MfdContentCollection *new_content){ mfd_content_collection=new_content; }
    MfdContentCollection*   getMfdContentCollection(){ return mfd_content_collection; } 
      
    QString                 getName(){return name;}
    QString                 getHost(){return host;}
    
    void                    setPreviousTreePosition(QStringList tree_list){previous_tree_position = tree_list;}
    QStringList             getPreviousTreePosition(){return previous_tree_position;}
    
    AudioMetadata*          getAudioMetadata(int collection_id, int item_id);

  private:
  
    int         id;
    QString     name;
    QString     host;

    MfdContentCollection *mfd_content_collection;
    QStringList previous_tree_position;
};


#endif
