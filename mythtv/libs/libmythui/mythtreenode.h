#ifndef MYTHTREENODE_H_
#define MYTHTREENODE_H_
/*
	mythtreenode.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An object for building trees of thing (mostly metadata/content)
*/

#include <qstring.h>

enum TreeNodeType
{
    TNT_unknown = 0,
    TNT_something
};




class MythTreeNode
{
  public:
  
    MythTreeNode(const QString &a_string);
   ~MythTreeNode();

  private:
  
    QString      m_string;
    TreeNodeType m_type;
    
};

#endif
