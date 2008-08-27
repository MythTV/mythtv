#ifndef MYTHGENERICTREE_H_
#define MYTHGENERICTREE_H_

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMetaType>

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

    MythGenericTree *findLeaf();

    MythGenericTree* findNode(QList<int> route_of_branches, int depth=-1);

    MythGenericTree *nextSibling(int number_down);
    MythGenericTree *prevSibling(int number_up);

    QList<MythGenericTree*>::iterator getFirstChildIterator();

    MythGenericTree *getSelectedChild();
    MythGenericTree *getChildAt(uint reference);
    MythGenericTree *getChildByName(const QString &a_name);
    MythGenericTree *getChildById(int an_int);

    QList<MythGenericTree*> *getAllChildren();

    int getChildPosition(MythGenericTree *child);
    int getPosition(void);

    QList<int> getRouteById(void);
    QStringList getRouteByString(void);

    void setInt(int an_int) { m_int = an_int; }
    int getInt() { return m_int; }

    void setParent(MythGenericTree* a_parent) { m_parent = a_parent; }
    MythGenericTree *getParent(void);

    const QString getString(void) { return m_string; }
    void setString(const QString &str) { m_string = str; }

    int calculateDepth(int start=0);

    int childCount(void);
    int siblingCount(void);

    void setSelectable(bool flag) { m_selectable = flag; }
    bool isSelectable() { return m_selectable; }

    void setAttribute(uint attribute_position, int value_of_attribute);
    int getAttribute(uint which_one);
    IntVector *getAttributes(void) { return m_attributes; }

    void setOrderingIndex(int ordering_index);
    int getOrderingIndex(void) { return m_currentOrderingIndex; }

    void becomeSelectedChild(void);
    void setSelectedChild(MythGenericTree* a_node) { m_selected_subnode = a_node; }

    void addYourselfIfSelectable(QList<MythGenericTree*> *flat_list);
    void buildFlatListOfSubnodes(bool scrambled_parents);

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
    void reorderSubnodes(void);

    QString m_string;
    int m_int;

    SortableMythGenericTreeList *m_subnodes;
    SortableMythGenericTreeList *m_ordered_subnodes;
    SortableMythGenericTreeList *m_flatenedSubnodes;

    MythGenericTree *m_selected_subnode;
    IntVector *m_attributes;
    MythGenericTree *m_parent;

    bool m_selectable;
    int m_currentOrderingIndex;
};

Q_DECLARE_METATYPE(MythGenericTree*)

#endif
