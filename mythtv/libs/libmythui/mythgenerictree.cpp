// C++/C headers
#include <iostream>

// Myth headers
#include "mythverbose.h"

// Mythui headers
#include "mythgenerictree.h"

class SortableMythGenericTreeList : public QList<MythGenericTree*>
{
  public:
    enum SortType {SORT_ATTRIBUTE=0, SORT_STRING=1, SORT_SELECTABLE=3,
                   SORT_ATT_THEN_STRING};

    void SetSortType(SortType stype) { m_sortType = stype; }
    void SetAttributeIndex(int index)
                                { m_attributeIndex = (index >= 0) ? index : 0; }

//     static bool sortByAttribute(MythGenericTree *one, MythGenericTree *two)
//     {
//         int onea = one->getAttribute(m_attributeIndex);
//         int twoa = two->getAttribute(m_attributeIndex);
//
//         if (onea == twoa)
//             return 0;
//         else if (onea < twoa)
//             return -1;
//         else
//             return 1;
//     }

    static bool sortByString(MythGenericTree *one, MythGenericTree *two)
    {
        QString ones = one->getString().toLower();
        QString twos = two->getString().toLower();
        return QString::localeAwareCompare(ones, twos);
    }

    static bool sortBySelectable(MythGenericTree *one, MythGenericTree *two)
    {
        bool onesel = one->isSelectable();
        bool twosel = two->isSelectable();

        if (onesel == twosel)
            return 0;
        else if (onesel && !twosel)
            return 1;
        else
            return -1;
    }

//     static bool sortByAttributeThenString(MythGenericTree *one, MythGenericTree *two)
//     {
//         //
//         //  Sort by attribute (ordering index), but, it if those are
//         //  equal, then sort by string
//         //
//
//         int onea = one->getAttribute(m_attributeIndex);
//         int twoa = two->getAttribute(m_attributeIndex);
//
//         if (onea == twoa)
//         {
//             QString ones = one->getString().toLower();
//             QString twos = two->getString().toLower();
//             return QString::localeAwareCompare(ones, twos);
//         }
//         else if (onea < twoa)
//             return -1;
//         else
//             return 1;
//     }

    void Sort(SortType stype, int attributeIndex = 0)
    {
        m_sortType = stype;
        m_attributeIndex = attributeIndex;
        switch (m_sortType)
        {
//             case SORT_ATTRIBUTE:
//                 std::sort(begin(), end(), sortByAttribute);
//                 break;
            case SORT_STRING:
                qSort(begin(), end(), sortByString);
                break;
            case SORT_SELECTABLE:
                qSort(begin(), end(), sortBySelectable);
                break;
//             case SORT_ATT_THEN_STRING:
//                 std::sort(begin(), end(), sortByAttributeThenString);
//                 break;
        }
    }

  private:
    SortType m_sortType;
    int m_attributeIndex; // for getAttribute
};

///////////////////////////////////////////////////////

MythGenericTree::MythGenericTree(const QString &a_string, int an_int,
                         bool selectable_flag)
{
    m_subnodes = new SortableMythGenericTreeList;
    m_ordered_subnodes = new SortableMythGenericTreeList;
    m_flatenedSubnodes = new SortableMythGenericTreeList;

    m_parent = NULL;
    m_selected_subnode = NULL;
    m_currentOrderingIndex = -1;

    // Use 6 here, because we know that's what mythmusic wants (limits resizing)
    m_attributes = new IntVector(6);

    m_string = a_string;
    m_int = an_int;
    m_selectable = selectable_flag;
}

MythGenericTree::~MythGenericTree()
{
    while (!m_subnodes->isEmpty())
        delete m_subnodes->takeFirst();
    delete m_subnodes;
    delete m_ordered_subnodes;
    delete m_flatenedSubnodes;
    delete m_attributes;
}

MythGenericTree* MythGenericTree::addNode(const QString &a_string, int an_int,
                                  bool selectable_flag)
{
    MythGenericTree *new_node = new MythGenericTree(a_string.simplified(),
                                            an_int, selectable_flag);

    return addNode(new_node);
}

MythGenericTree *MythGenericTree::addNode(MythGenericTree *child)
{
    child->setParent(this);
    m_subnodes->append(child);
    m_ordered_subnodes->append(child);

    return child;
}

void MythGenericTree::removeNode(MythGenericTree *child)
{
    if (m_selected_subnode == child)
        m_selected_subnode = NULL;

    m_ordered_subnodes->removeAll(child);
    m_flatenedSubnodes->removeAll(child);
    m_subnodes->removeAll(child);
}

int MythGenericTree::calculateDepth(int start)
{
    int current_depth;
    int found_depth;
    current_depth = start + 1;

    QList<MythGenericTree*> *children = getAllChildren();
    if (children && children->count() > 0)
    {
        SortableMythGenericTreeList::Iterator it;
        MythGenericTree *child = NULL;

        for (it = children->begin(); it != children->end(); ++it)
        {
            child = *it;
            if (!child)
                continue;
            found_depth = child->calculateDepth(start + 1);
            if (found_depth > current_depth)
                current_depth = found_depth;
        }
    }

    return current_depth;
}

MythGenericTree* MythGenericTree::findLeaf()
{
    if (m_subnodes->count() > 0)
    {
        if (m_currentOrderingIndex == -1)
            return m_subnodes->first()->findLeaf();

        MythGenericTree *first_child = getChildAt(0);

        return first_child->findLeaf();
    }

    return this;
}

MythGenericTree* MythGenericTree::findNode(QList<int> route_of_branches,
                                           int depth)
{
    // Starting from *this* node (which will often be root) find a set of
    // branches that have id's that match the collection passed in
    // route_of_branches. Return the end point of those branches.
    //
    // In practical terms, mythmusic will use this to force the playback
    // screen's ManagedTreeList to move to a given track in a given playlist

    MythGenericTree *node = NULL;
    for (int i = 0; i < route_of_branches.count(); i++)
    {
        if (!node)
            node = this;

        bool foundit = false;
        QList<MythGenericTree*>::iterator it;
        QList<MythGenericTree*> *children = node->getAllChildren();

        if (!children)
            break;

        MythGenericTree *child = NULL;

        for (it = children->begin(); it != children->end(); ++it)
        {
            child = *it;
            if (!child)
                continue;
            if (child->getInt() == route_of_branches[i])
            {
                node = child;
                foundit = true;
                break;
            }
        }

        if (!foundit)
            break;
    }

    return NULL;
}

int MythGenericTree::getChildPosition(MythGenericTree *child)
{
    if (m_currentOrderingIndex == -1)
        return m_subnodes->indexOf(child);

    return m_ordered_subnodes->indexOf(child);
}

int MythGenericTree::getPosition()
{
    if (m_parent)
        return m_parent->getChildPosition(this);
    return 0;
}

QList<int> MythGenericTree::getRouteById()
{
    QList<int> routeByID;

    routeByID.push_front(getInt());

    MythGenericTree *parent = this;
    while( (parent = parent->getParent()) )
    {
        routeByID.push_front(parent->getInt());
    }
    return routeByID;
}

QStringList MythGenericTree::getRouteByString()
{
    QStringList routeByString;

    routeByString.push_front(getString());

    MythGenericTree *parent = this;
    while( (parent = parent->getParent()) )
    {
        routeByString.push_front(parent->getString());
    }
    return routeByString;
}

int MythGenericTree::childCount(void)
{
    return m_subnodes->count();
}

int MythGenericTree::siblingCount(void)
{
    if (m_parent)
        return m_parent->childCount();
    return 1;
}

QList<MythGenericTree*> *MythGenericTree::getAllChildren()
{
    if (m_currentOrderingIndex == -1)
        return m_subnodes;

    return m_ordered_subnodes;
}

MythGenericTree* MythGenericTree::getChildAt(uint reference)
{
    if (reference >= (uint)m_ordered_subnodes->count())
        return NULL;

    if (m_currentOrderingIndex == -1)
        return m_subnodes->at(reference);

    return m_ordered_subnodes->at(reference);
}

MythGenericTree* MythGenericTree::getSelectedChild()
{
    if (m_selected_subnode)
        return m_selected_subnode;
    return getChildAt(0);
}

void MythGenericTree::becomeSelectedChild()
{
    if (m_parent)
        m_parent->setSelectedChild(this);
    else
        VERBOSE(VB_IMPORTANT, "Top level can't become selected child");
}

MythGenericTree* MythGenericTree::prevSibling(int number_up)
{
    if (!m_parent)
    {
        // I'm root = no siblings
        return NULL;
    }

    int position = m_parent->getChildPosition(this);

    if (position < number_up)
    {
        // not enough siblings "above" me
        return NULL;
    }

    return m_parent->getChildAt(position - number_up);
}

MythGenericTree* MythGenericTree::nextSibling(int number_down)
{
    if (!m_parent)
    {
        // I'm root = no siblings
        return NULL;
    }

    int position = m_parent->getChildPosition(this);

    if (position + number_down >= m_parent->childCount())
    {
        // not enough siblings "below" me
        return NULL;
    }

    return m_parent->getChildAt(position + number_down);
}

QList<MythGenericTree*>::iterator MythGenericTree::getFirstChildIterator()
{
    if (m_currentOrderingIndex == -1)
    {
        QList<MythGenericTree*>::iterator it;
        it = m_subnodes->begin();
        return it;
    }

    QList<MythGenericTree*>::iterator it;
    it = m_ordered_subnodes->begin();
    return it;
}

MythGenericTree* MythGenericTree::getParent()
{
    if (m_parent)
        return m_parent;
    return NULL;
}

void MythGenericTree::setAttribute(uint attribute_position, int value_of_attribute)
{
    // You can use attibutes for anything you like. Mythmusic, for example,
    // stores a value for random ordering in the first "column" (0) and a value
    // for "intelligent" (1) ordering in the second column

    if (m_attributes->size() < (int)(attribute_position + 1))
        m_attributes->resize(attribute_position + 1);
    (*m_attributes)[attribute_position] = value_of_attribute;
}

int MythGenericTree::getAttribute(uint which_one)
{
    if (m_attributes->size() < (int)(which_one + 1))
    {
        VERBOSE(VB_IMPORTANT, "asked a MythGenericTree node for a nonexistant"
                              "attribute");
        return 0;
    }

    return m_attributes->at(which_one);
}

void MythGenericTree::setOrderingIndex(int ordering_index)
{
    m_currentOrderingIndex = ordering_index;
    reorderSubnodes();
}

void MythGenericTree::reorderSubnodes()
{
    // The nodes are there, we just want to re-order them according to
    // attribute column defined by ordering_index

    m_ordered_subnodes->Sort(SortableMythGenericTreeList::SORT_ATTRIBUTE,
                             m_currentOrderingIndex);
}

void MythGenericTree::addYourselfIfSelectable(QList<MythGenericTree*> *flat_list)
{
    if (m_selectable)
        flat_list->append(this);

    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        child->addYourselfIfSelectable(flat_list);
        ++it;
    }
}

void MythGenericTree::buildFlatListOfSubnodes(bool scrambled_parents)
{
    // This builds a flat list of every selectable child according to the
    // current ordering index.

    m_flatenedSubnodes->clear();

    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        child->addYourselfIfSelectable(m_flatenedSubnodes);
        ++it;
    }

    if (m_currentOrderingIndex > -1)
        m_flatenedSubnodes->SetAttributeIndex(m_currentOrderingIndex);
}

MythGenericTree* MythGenericTree::nextPrevFromFlatList(bool forward_or_backward,
                                               bool wrap_around,
                                               MythGenericTree *active)
{
    int i = m_flatenedSubnodes->indexOf(active);
    if (i < 0)
    {
        VERBOSE(VB_IMPORTANT, "Can't find active item on flatened list");
        return NULL;
    }

    if (forward_or_backward)
    {
        ++i;
        if (i >= (int)m_flatenedSubnodes->count())
        {
            if (wrap_around)
                i = 0;
            else
                return NULL;
        }
    }
    else
    {
        --i;
        if (i < 0)
        {
            if (wrap_around)
                i = m_flatenedSubnodes->count() - 1;
            else
                return NULL;
        }
    }

    return m_flatenedSubnodes->at(i);
}

MythGenericTree* MythGenericTree::getChildByName(const QString &a_name)
{
    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        if (child->getString() == a_name)
            return child;
        ++it;
    }
    return NULL;
}

MythGenericTree* MythGenericTree::getChildById(int an_int)
{
    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        if (child->getInt() == an_int)
            return child;
        ++it;
    }
    return NULL;
}

void MythGenericTree::sortByString()
{
    m_ordered_subnodes->Sort(SortableMythGenericTreeList::SORT_STRING);

    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        child->sortByString();
        ++it;
    }
}

void MythGenericTree::sortByAttributeThenByString(int which_attribute)
{
    m_ordered_subnodes->Sort(SortableMythGenericTreeList::SORT_ATT_THEN_STRING,
                             which_attribute);

    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        child->sortByAttributeThenByString(which_attribute);
        ++it;
    }
}

void MythGenericTree::sortBySelectable()
{
    m_ordered_subnodes->Sort(SortableMythGenericTreeList::SORT_SELECTABLE);

    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        child->sortBySelectable();
        ++it;
    }
}

void MythGenericTree::deleteAllChildren()
{
    m_flatenedSubnodes->clear();
    m_ordered_subnodes->clear();
    m_selected_subnode = NULL;
    m_currentOrderingIndex = -1;
    while (!m_subnodes->isEmpty())
        delete m_subnodes->takeFirst();
    m_subnodes->clear();
}

void MythGenericTree::reOrderAsSorted()
{
    //
    //  Arrange (recursively) my subnodes in the same order as my ordered
    //  subnodes
    //

    if (m_subnodes->count() != m_ordered_subnodes->count())
    {
        VERBOSE(VB_IMPORTANT, "Can't reOrderAsSorted(), because the number "
             "of subnodes is different than the number of ordered subnodes");
        return;
    }

    m_subnodes->clear();
    m_currentOrderingIndex = -1;

    QList<MythGenericTree*>::iterator it;
    it = m_ordered_subnodes->begin();
    MythGenericTree *child;
    while ((child = *it) != 0)
    {
        m_subnodes->append(child);
        child->reOrderAsSorted();
        ++it;
    }
}

void MythGenericTree::MoveItemUpDown(MythGenericTree *item, bool flag)
{
    if (item == m_subnodes->first() && flag)
        return;
    if (item == m_subnodes->last() && !flag)
        return;

    int num = m_subnodes->indexOf(item);

    int insertat = 0;
    if (flag)
        insertat = num - 1;
    else
        insertat = num + 1;

    m_subnodes->removeAt(num);
    m_subnodes->insert(insertat, item);
}
