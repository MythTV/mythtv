/*
	mythuitree.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An object that can display a tree (hopefully in many useful ways)
*/

#include <iostream>
using namespace std;

#include <qintdict.h>

#include "mythuitree.h"
#include "mythuitreecolumn.h"

class MythUITreePrivate
{
  public:
  
    MythUITreePrivate(QRect displayRect, MythUITree *l_owner);
    ~MythUITreePrivate();

    void setTree(MythTreeNode *l_tree_root);
    void preInit(MythUITreeType l_tree_type, int numb_cols, int numb_rows);
    void addColumn(int column_id, QRect area, bool debug_on);
    
  private:
    
    QIntDict<MythUITreeColumn>  m_columns;
    MythTreeNode               *m_tree_root;
    QRect                       m_Area;
    MythUITreeType              m_tree_type;
    int                         m_expected_columns;
    int                         m_expected_rows;
    MythUITree                 *m_owner;

};

MythUITree::MythUITree(QRect displayRect, MythUIType *parent, const char *name)
           :MythUIType(parent, name)
{
    //
    //  Store my area
    //

    m_Area = displayRect;
    d = new MythUITreePrivate(displayRect, this);
    setDebugColor(QColor(255,255,0));

}

void MythUITree::setTree(MythTreeNode *l_tree_root)
{
    d->setTree(l_tree_root);
}

void MythUITree::preInit(MythUITreeType l_tree_type, int numb_cols, int numb_rows)
{
    d->preInit(l_tree_type, numb_cols, numb_rows);
}

void MythUITree::addColumn(int column_id, QRect area, bool debug_on)
{
    d->addColumn(column_id, area, debug_on);
}

MythUITree::~MythUITree()
{
    if(d)
    {
        delete d;
    }
}

/*
---------------------------------------------------------------------
*/

MythUITreePrivate::MythUITreePrivate(QRect displayRect, MythUITree *l_owner)
{
    //
    //  Set defaults
    //
    
    m_tree_type = MUTT_vertical;
    m_expected_columns = 0;
    m_expected_rows = 0;
    
    m_tree_root = NULL;
    m_Area = displayRect;
    m_owner = l_owner;
}

void MythUITreePrivate::setTree(MythTreeNode *l_tree_root)
{
    m_tree_root = l_tree_root;
}

void MythUITreePrivate::preInit(MythUITreeType l_tree_type, int numb_cols, int numb_rows)
{
    //
    //  This is called to set things (before things like columns are added)
    //
    
    m_tree_type = l_tree_type;
    m_expected_columns = numb_cols;
    m_expected_rows = numb_rows;
}

void MythUITreePrivate::addColumn(int column_id, QRect area, bool debug_on)
{
    //
    //  Sanity check 
    //
    
    if(column_id < 1)
    {
        cerr << "mythuitree.o: was asked to add a column "
             << "with an id less than 0 to tree (ignoring)"
             << endl;
        return;
    }

    if(column_id > m_expected_columns)
    {
        cerr << "mythuitree.o: was asked to add a column "
             << "with an id greater than the expected "
             << "number of columns (ignoring)"
             << endl;
        return;
    }

    if(m_columns[column_id])
    {
        cerr << "mythuitree.o: was asked to add a column "
             << "with an id of "
             << column_id
             << ", but that column already exists (ignoring)"
             << endl;
        return;
    }

    //
    //  Does this column lie inside my area?
    //
    
    if(!m_Area.contains(area))
    {
        //
        //  Uh, oh
        //
        
        cerr << "mythuitree.o: column with id of "
             << column_id
             << " does not lie inside tree area (clipping)"
             << endl;
        if(area.x() < m_Area.x())
        {
            area.setX(m_Area.x());
        }
        if(area.y() < m_Area.y())
        {
            area.setY(m_Area.y());
        }
        if(area.right() > m_Area.right())
        {
            area.setRight(m_Area.right());
        }
        if(area.bottom() > m_Area.bottom())
        {
            area.setBottom(m_Area.bottom());
        }
    }
    
    MythUITreeColumn *new_column = new MythUITreeColumn(
                                                        area,
                                                        column_id,
                                                        m_owner,
                                                        QString("Column %1")
                                                        .arg(column_id)
                                                       );    

    //
    //  Check for debugging
    //
    
    if(debug_on)
    {
        new_column->setDebug(true);
    }
    
    //
    //  Have the column sort out how many buttons it can display
    //
    
    new_column->calculateMaxButtons();

    //
    //  Put into my dictionary, so that I can keep track of all the columns
    //  as well
    //

    m_columns.insert(column_id, new_column);
}

MythUITreePrivate::~MythUITreePrivate()
{
}


