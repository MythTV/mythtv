#ifndef MYTHUITREE_H_
#define MYTHUITREE_H_
/*
	mythuitree.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An object that can display a tree (hopefully in many useful ways)
*/


#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythtreenode.h"

class MythUITreePrivate;

enum MythUITreeType
{
    MUTT_vertical = 0,
    MUTT_horizontal,
    MUTT_folder
};



class MythUITree : public MythUIType
{
  public:
  
    MythUITree(QRect displayRect, MythUIType *parent, const char *name);
   ~MythUITree();
   
    void setTree(MythTreeNode *l_tree_root);
    void preInit(MythUITreeType l_tree_type, int numb_cols, int num_rows);
    void addColumn(int column_id, QRect area, bool debug_on);

  private:
  
    MythUITreeType     m_tree_type;
    MythUITreePrivate *d;
    
};

#endif
