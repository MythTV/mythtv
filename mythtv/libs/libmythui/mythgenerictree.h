#ifndef MYTHGENERICTREE_H_
#define MYTHGENERICTREE_H_

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMetaType>
#include <QVariant>
#include <QMap>
#include <QHash>

#include "mythexp.h"

#include "mythuibuttonlist.h"

class SortableMythGenericTreeList;

class MPUBLIC MythGenericTree
{
    typedef QVector<int> IntVector;

  public:
    MythGenericTree(const QString &a_string = "", int an_int = 0,
                bool selectable_flag = false);
    virtual ~MythGenericTree();

    MythGenericTree *addNode(const QString &a_string, int an_int = 0,
                         bool selectable_flag = false, bool visible = true);
    MythGenericTree *addNode(MythGenericTree *child);

    /// Detach this node/branch from it's parent without deleting it, it can
    /// then be reattached elsewhere or used on it's own
    void DetachParent(void);

    void removeNode(MythGenericTree *child);
    void deleteNode(MythGenericTree *child);

    MythGenericTree *findLeaf();
    MythGenericTree* findNode(QList<int> route_of_branches, int depth=-1);

    MythGenericTree *nextSibling(int number_down);
    MythGenericTree *prevSibling(int number_up);

    QList<MythGenericTree*>::iterator getFirstChildIterator() const;

    MythGenericTree *getSelectedChild(bool onlyVisible = false) const;
    MythGenericTree *getVisibleChildAt(uint reference) const;
    MythGenericTree *getChildAt(uint reference) const;
    MythGenericTree *getChildByName(const QString &a_name) const;
    MythGenericTree *getChildById(int an_int) const;

    QList<MythGenericTree*> *getAllChildren() const;

    int getChildPosition(MythGenericTree *child) const;
    int getPosition(void);

    QList<int> getRouteById(void);
    QStringList getRouteByString(void);
    QList<MythGenericTree*> getRoute(void);

    void setInt(int an_int) { m_int = an_int; }
    int getInt() const { return m_int; }

    void setParent(MythGenericTree* a_parent) { m_parent = a_parent; }
    MythGenericTree *getParent(void) const;

    // Deprecated
    const QString getString(void) const { return m_text; }
    void setString(const QString &str) { m_text = str; }
    // End deprecated

    void SetText(const QString &text, const QString &name="",
                 const QString &state="");
    void SetTextFromMap(QHash<QString, QString> &infoMap, const QString &state="");
    QString GetText(const QString &name="") const;

    void SetImage(const QString &filename, const QString &name="");
    QString GetImage(const QString &name="") const;

    void SetData(QVariant data) { m_data = data; }
    const QVariant GetData(void) const { return m_data; }

    int calculateDepth(int start=0);

    int childCount(void) const;
    uint visibleChildCount() const { return m_visibleCount; }
    int siblingCount(void) const;

    void setSelectable(bool flag) { m_selectable = flag; }
    bool isSelectable() const { return m_selectable; }

    void SetVisible(bool visible);
    bool IsVisible() const { return m_visible; }

    void IncVisibleCount() { m_visibleCount++; }
    void DecVisibleCount() { m_visibleCount--; }

    void setAttribute(uint attribute_position, int value_of_attribute);
    int getAttribute(uint which_one) const;
    IntVector *getAttributes(void) const { return m_attributes; }

    void setOrderingIndex(int ordering_index);
    int getOrderingIndex(void) const { return m_currentOrderingIndex; }

    void becomeSelectedChild(void);
    void setSelectedChild(MythGenericTree* a_node) { m_selected_subnode = a_node; }

    void addYourselfIfSelectable(QList<MythGenericTree*> *flat_list);
    void buildFlatListOfSubnodes(bool scrambled_parents);

    MythGenericTree *nextPrevFromFlatList(bool forward_or_back, bool wrap_around,
                                      MythGenericTree *active) const;

    void sortByString();
    void sortByAttributeThenByString(int which_attribute);
    void sortBySelectable();
    void deleteAllChildren();
    void reOrderAsSorted();

    // only changes m_subnodes.  resort it if you want the others to change
    void MoveItemUpDown(MythGenericTree *item, bool flag);

    virtual MythUIButtonListItem *CreateListButton(MythUIButtonList *list);

  private:
    void reorderSubnodes(void);

    QString m_text;
    QMap<QString, TextProperties> m_strings;
    QMap<QString, QString> m_imageFilenames;
    QMap<QString, QString> m_states;
    int m_int;
    QVariant m_data;
    uint m_visibleCount;

    SortableMythGenericTreeList *m_subnodes;
    SortableMythGenericTreeList *m_ordered_subnodes;
    SortableMythGenericTreeList *m_flatenedSubnodes;

    MythGenericTree *m_selected_subnode;
    IntVector *m_attributes;
    MythGenericTree *m_parent;

    bool m_selectable;
    bool m_visible;
    int m_currentOrderingIndex;
};

Q_DECLARE_METATYPE(MythGenericTree*)

#endif
