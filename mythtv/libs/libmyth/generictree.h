#ifndef GENERICTREE_H_
#define GENERICTREE_H_

#include <qstring.h>
#include <qvaluevector.h>
#include <qvaluelist.h>
#include <qptrlist.h>

#include "mythexp.h"

using namespace std;

class SortableGenericTreeList;

class MPUBLIC GenericTree
{
    typedef QValueVector<int> IntVector;

  public:
    GenericTree(const QString &a_string = "", int an_int = 0, 
                bool selectable_flag = false);
    virtual ~GenericTree();

    GenericTree *addNode(const QString &a_string, int an_int = 0, 
                         bool selectable_flag = false);
    GenericTree *addNode(GenericTree *child);

    void removeNode(GenericTree *child);

    GenericTree *findLeaf(int ordering_index = -1);

    GenericTree* findNode(QValueList<int> route_of_branches);
    GenericTree* recursiveNodeFinder(QValueList<int> route_of_branches);
    bool checkNode(QValueList<int> route_of_branches);

    GenericTree *nextSibling(int number_down, int ordering_index = -1);
    GenericTree *prevSibling(int number_up, int ordering_index = -1);

    QPtrListIterator<GenericTree> getFirstChildIterator(int ordering = -1);

    GenericTree *getSelectedChild(int ordering_index);
    GenericTree *getChildAt(uint reference, int ordering_index = -1);
    GenericTree *getChildByName(const QString &a_name);
    GenericTree *getChildByInt(int an_int);

    QPtrList<GenericTree> *getAllChildren(int ordering_index = -1);

    int getChildPosition(GenericTree *child, int ordering_index = -1);
    int getPosition(void);
    int getPosition(int ordering_index);

    void setInt(int an_int) { m_int = an_int; }
    int getInt() { return m_int; }

    void setParent(GenericTree* a_parent) { m_parent = a_parent; }
    GenericTree *getParent(void);

    const QString getString(void) { return m_string; }
    void setString(const QString &str) { m_string = str; }

    int calculateDepth(int start);

    int childCount(void);
    int siblingCount(void);

    void setSelectable(bool flag) { m_selectable = flag; }
    bool isSelectable() { return m_selectable; }

    void setAttribute(uint attribute_position, int value_of_attribute);
    int getAttribute(uint which_one);
    IntVector *getAttributes(void) { return m_attributes; }

    void reorderSubnodes(int ordering_index);
    void setOrderingIndex(int ordering_index)
                 { m_current_ordering_index = ordering_index; }
    int getOrderingIndex(void) { return m_current_ordering_index; }

    void becomeSelectedChild(void);
    void setSelectedChild(GenericTree* a_node) { m_selected_subnode = a_node; }

    void addYourselfIfSelectable(QPtrList<GenericTree> *flat_list);
    void buildFlatListOfSubnodes(int ordering_index, bool scrambled_parents);

    GenericTree *nextPrevFromFlatList(bool forward_or_back, bool wrap_around, 
                                      GenericTree *active);

    void sortByString();
    void sortByAttributeThenByString(int which_attribute);
    void sortBySelectable();
    void deleteAllChildren();
    void pruneAllChildren();
    void reOrderAsSorted();

    // only changes m_subnodes.  resort it if you want the others to change
    void MoveItemUpDown(GenericTree *item, bool flag);

  private:
    QString m_string;
    int m_int;

    SortableGenericTreeList *m_subnodes;
    SortableGenericTreeList *m_ordered_subnodes;
    SortableGenericTreeList *m_flatened_subnodes;

    GenericTree *m_selected_subnode;
    IntVector *m_attributes;
    GenericTree *m_parent;

    bool m_selectable;
    int m_current_ordering_index;
};

#endif
