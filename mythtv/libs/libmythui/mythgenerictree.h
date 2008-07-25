#ifndef MYTHGENERICTREE_H_
#define MYTHGENERICTREE_H_

#include <QString>
#include <QList>
#include <QVector>

#include "mythexp.h"

using namespace std;

class SortableMythGenericTreeList;

class MPUBLIC MythGenericTree
{
    typedef QVector<int> IntVector;

  public:
    MythGenericTree(const QString &a_string = "", int an_int = 0,
                bool selectable_flag = false);
    virtual ~MythGenericTree();

    MythGenericTree *addNode(const QString &a_string, int an_int = 0,
                         bool selectable_flag = false);
    MythGenericTree *addNode(MythGenericTree *child);

    void removeNode(MythGenericTree *child);

    MythGenericTree *findLeaf(int ordering_index = -1);

    MythGenericTree* findNode(QList<int> route_of_branches);
    MythGenericTree* recursiveNodeFinder(QList<int> route_of_branches);
    bool checkNode(QList<int> route_of_branches);

    MythGenericTree *nextSibling(int number_down, int ordering_index = -1);
    MythGenericTree *prevSibling(int number_up, int ordering_index = -1);

    QList<MythGenericTree*>::iterator getFirstChildIterator(int ordering = -1);

    MythGenericTree *getSelectedChild(int ordering_index);
    MythGenericTree *getChildAt(uint reference, int ordering_index = -1);
    MythGenericTree *getChildByName(const QString &a_name);
    MythGenericTree *getChildByInt(int an_int);

    QList<MythGenericTree*> *getAllChildren(int ordering_index = -1);

    int getChildPosition(MythGenericTree *child, int ordering_index = -1);
    int getPosition(void);
    int getPosition(int ordering_index);

    void setInt(int an_int) { m_int = an_int; }
    int getInt() { return m_int; }

    void setParent(MythGenericTree* a_parent) { m_parent = a_parent; }
    MythGenericTree *getParent(void);

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
    void setSelectedChild(MythGenericTree* a_node) { m_selected_subnode = a_node; }

    void addYourselfIfSelectable(QList<MythGenericTree*> *flat_list);
    void buildFlatListOfSubnodes(int ordering_index, bool scrambled_parents);

    MythGenericTree *nextPrevFromFlatList(bool forward_or_back, bool wrap_around,
                                      MythGenericTree *active);

    void sortByString();
    void sortByAttributeThenByString(int which_attribute);
    void sortBySelectable();
    void deleteAllChildren();
    void reOrderAsSorted();

    // only changes m_subnodes.  resort it if you want the others to change
    void MoveItemUpDown(MythGenericTree *item, bool flag);

  private:
    QString m_string;
    int m_int;

    SortableMythGenericTreeList *m_subnodes;
    SortableMythGenericTreeList *m_ordered_subnodes;
    SortableMythGenericTreeList *m_flatened_subnodes;

    MythGenericTree *m_selected_subnode;
    IntVector *m_attributes;
    MythGenericTree *m_parent;

    bool m_selectable;
    int m_current_ordering_index;
};

#endif
