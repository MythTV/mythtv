#ifndef MFDINFO_H_
#define MFDINFO_H_
/*
	mfdinfo.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qstring.h>

#include <mythtv/generictree.h>

class MfdInfo
{

  public:

    MfdInfo(int an_id, const QString &a_name, const QString &a_host);
    ~MfdInfo();

    int  getId(){ return id; }
    void setMetadata(GenericTree *new_tree){ tree_data=new_tree; }
    
    QString getName(){return name;}
    QString getHost(){return host;}

  private:
  
    int         id;
    QString     name;
    QString     host;
    GenericTree *tree_data;
};


#endif
