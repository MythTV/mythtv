#ifndef MFDINFO_H_
#define MFDINFO_H_
/*
	mfdinfo.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qstring.h>
#include <qstringlist.h>

#include <mythtv/generictree.h>

class MfdInfo
{

  public:

    MfdInfo(int an_id, const QString &a_name, const QString &a_host);
    ~MfdInfo();

    int          getId(){ return id; }

    void         setMetadata(GenericTree *new_tree){ tree_data=new_tree; }
    GenericTree* getMetadata(){ return tree_data; } 
      
    QString      getName(){return name;}
    QString      getHost(){return host;}
    
    void         setPreviousTreePosition(QStringList tree_list){previous_tree_position = tree_list;}
    QStringList  getPreviousTreePosition(){return previous_tree_position;}

  private:
  
    int         id;
    QString     name;
    QString     host;
    GenericTree *tree_data;
    QStringList previous_tree_position;
};


#endif
