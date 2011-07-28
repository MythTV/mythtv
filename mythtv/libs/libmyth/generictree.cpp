#include <QMutex>
#include <iostream>
#include <algorithm>
using namespace std;

#include "generictree.h"
#include "mythlogging.h"

#define LOC      QString("*TreeList: ")

class SortableGenericTreeList : public vector<GenericTree*>
{
  public:
    void SetSortType(int stype) { sort_type = stype; }
    void SetOrderingIndex(int oindex)
    { ordering_index = (oindex >= 0) ? oindex : 0; }

    void sort(void);

  private:
    int sort_type; // 0 - getAttribute
                   // 1 - getString
                   // 2 - isSelectable
                   // 3 - attribute, then string
    int ordering_index; // for getAttribute
};

static int compareItems(
    int sort_type, int ordering_index,
    const GenericTree *one, const GenericTree *two)
{
    if (sort_type == 0)
    {
        int onea = one->getAttribute(ordering_index);
        int twoa = two->getAttribute(ordering_index);

        if (onea == twoa)
            return 0;
        else if (onea < twoa)
            return -1;
        else
            return 1;
    }
    else if (sort_type == 1)
    {
        QString ones = one->getString().toLower();
        QString twos = two->getString().toLower();
        return QString::localeAwareCompare(ones, twos);
    }
    else if (sort_type == 2)
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
    if (sort_type == 3)
    {
        //
        //  Sort by attribute (ordering index), but, it if those are
        //  equal, then sort by string
        //

        int onea = one->getAttribute(ordering_index);
        int twoa = two->getAttribute(ordering_index);

        if (onea == twoa)
        {
            QString ones = one->getString().toLower();
            QString twos = two->getString().toLower();
            return QString::localeAwareCompare(ones, twos);
        }
        else if (onea < twoa)
            return -1;
        else
            return 1;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ALERT, "SortableGenericTreeList was asked to "
                "compare items (probably inside a sort()), but the "
                "sort_type is not set to anything recognizable");
        return 0;
    }
}

static QMutex global_generic_sort_lock;
static int global_stype;
static int global_oindex;

static inline bool generic_tree_lt(const GenericTree *a, const GenericTree *b)
{
    return compareItems(global_stype, global_oindex, a, b) < 0;
}

void SortableGenericTreeList::sort(void)
{
    QMutexLocker locker(&global_generic_sort_lock);
    global_stype  = sort_type;
    global_oindex = ordering_index;
    std::stable_sort(begin(), end(), generic_tree_lt);
}

GenericTree::GenericTree(const QString &a_string, int an_int,
                         bool selectable_flag)
{
    m_subnodes = new SortableGenericTreeList;
    m_ordered_subnodes = new SortableGenericTreeList;
    m_flatened_subnodes = new SortableGenericTreeList;

    m_parent = NULL;
    m_selected_subnode = NULL;
    m_current_ordering_index = -1;

    // Use 6 here, because we know that's what mythmusic wants (limits resizing)
    m_attributes = new IntVector(6);

    m_string = a_string;
    m_int = an_int;
    m_selectable = selectable_flag;
}

GenericTree::~GenericTree()
{
    deleteAllChildren(true);
    delete m_subnodes;
    delete m_ordered_subnodes;
    delete m_flatened_subnodes;
    delete m_attributes;
}

GenericTree* GenericTree::addNode(const QString &a_string, int an_int,
                                  bool selectable_flag)
{
    GenericTree *new_node = new GenericTree(
        a_string.trimmed(), an_int, selectable_flag);

    return addNode(new_node);
}

GenericTree *GenericTree::addNode(GenericTree *child)
{
    child->setParent(this);
    m_subnodes->push_back(child);
    m_ordered_subnodes->push_back(child);

    return child;
}

void GenericTree::removeNode(GenericTree *child)
{
    if (m_selected_subnode == child)
        m_selected_subnode = NULL;

    vector<GenericTree*>::iterator oit =
        std::find(m_ordered_subnodes->begin(),
                  m_ordered_subnodes->end(), child);
    vector<GenericTree*>::iterator fit =
        std::find(m_flatened_subnodes->begin(),
                  m_flatened_subnodes->end(), child);
    vector<GenericTree*>::iterator it =
        std::find(m_subnodes->begin(), m_subnodes->end(), child);

    if (oit != m_ordered_subnodes->end())
        m_ordered_subnodes->erase(oit);
    if (fit != m_flatened_subnodes->end())
        m_flatened_subnodes->erase(fit);
    if (it != m_subnodes->end())
    {
        delete *it;
        m_subnodes->erase(it);
    }
}

int GenericTree::calculateDepth(int start)
{
    int current_depth = start + 1;
    int found_depth;

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
    {
        found_depth = (*it)->calculateDepth(start + 1);
        if (found_depth > current_depth)
            current_depth = found_depth;
    }

    return current_depth;
}

GenericTree* GenericTree::findLeaf(int ordering_index)
{
    if (!m_subnodes->empty())
    {
        if (ordering_index == -1)
            return m_subnodes->front()->findLeaf();

        GenericTree *first_child = getChildAt(0, ordering_index);

        return first_child->findLeaf(ordering_index);
    }

    return this;
}

GenericTree* GenericTree::findNode(QList<int> route_of_branches)
{
    // Starting from *this* node (which will often be root) find a set of
    // branches that have id's that match the collection passed in
    // route_of_branches. Return the end point of those branches (which will
    // often be a leaf node).
    //
    // In practical terms, mythmusic will use this to force the playback
    // screen's ManagedTreeList to move to a given track in a given playlist

    return recursiveNodeFinder(route_of_branches);
}

GenericTree* GenericTree::recursiveNodeFinder(QList<int> route_of_branches)
{
    if (checkNode(route_of_branches))
        return this;

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
    {
        GenericTree *sub_checker=(*it)->recursiveNodeFinder(route_of_branches);
        if (sub_checker)
            return sub_checker;
    }

    return NULL;
}

bool GenericTree::checkNode(QList<int> route_of_branches)
{
    bool found_it = true;
    GenericTree *parent_finder = this;

    // FIXME: slow

    for (int i = route_of_branches.count() - 1; i > -1 && found_it; --i)
    {
        if (parent_finder->getInt() != route_of_branches[i])
            found_it = false;

        if (i > 0)
        {
            if (parent_finder->getParent())
                parent_finder = parent_finder->getParent();
            else
                found_it = false;
        }
    }

    return found_it;
}

int GenericTree::getChildPosition(GenericTree *child, int ordering_index)
{
    if (ordering_index == -1)
    {
        vector<GenericTree*>::iterator it =
            std::find(m_subnodes->begin(), m_subnodes->end(), child);
        return (it != m_subnodes->end()) ? it - m_subnodes->begin() : -1;
    }

    if (m_current_ordering_index != ordering_index)
    {
        reorderSubnodes(ordering_index);
        m_current_ordering_index = ordering_index;
    }

    vector<GenericTree*>::iterator it =
        std::find(m_ordered_subnodes->begin(),
                  m_ordered_subnodes->end(), child);
    return (it != m_ordered_subnodes->end()) ?
        it - m_ordered_subnodes->begin() : -1;
}

int GenericTree::getPosition()
{
    if (m_parent)
        return m_parent->getChildPosition(this);
    return 0;
}

int GenericTree::getPosition(int ordering_index)
{
    if (m_parent)
        return m_parent->getChildPosition(this, ordering_index);
    return 0;
}

int GenericTree::childCount(void)
{
    return m_subnodes->size();
}

int GenericTree::siblingCount(void)
{
    if (m_parent)
        return m_parent->childCount();
    return 1;
}

vector<GenericTree*> GenericTree::getAllChildren(int ordering_index)
{
    vector<GenericTree*> list;

    if (ordering_index == -1)
    {
        SortableGenericTreeList::iterator it = m_subnodes->begin();
        for (; it != m_subnodes->end(); ++it)
            list.push_back(*it);
    }
    else
    {
        SortableGenericTreeList::iterator it = m_ordered_subnodes->begin();
        for (; it != m_ordered_subnodes->end(); ++it)
            list.push_back(*it);
    }

    return list;
}

GenericTree* GenericTree::getChildAt(uint reference, int ordering_index)
{
    if (reference >= (uint)m_ordered_subnodes->size())
    {
#if 0
        LOG(VB_GENERAL, LOG_ALERT,
                 "GenericTree: out of bounds request to getChildAt()");
#endif
        return NULL;
    }

    if (ordering_index == -1)
        return (*m_subnodes)[reference];

    if (ordering_index != m_current_ordering_index)
    {
        reorderSubnodes(ordering_index);
        m_current_ordering_index = ordering_index;
    }

    return m_ordered_subnodes->at(reference);
}

GenericTree* GenericTree::getSelectedChild(int ordering_index)
{
    if (m_selected_subnode)
        return m_selected_subnode;
    return getChildAt(0, ordering_index);
}

void GenericTree::becomeSelectedChild()
{
    if (m_parent)
        m_parent->setSelectedChild(this);
    else
        LOG(VB_GENERAL, LOG_ALERT, 
                 "Top level can't become selected child");
}

GenericTree* GenericTree::prevSibling(int number_up, int ordering_index)
{
    if (!m_parent)
    {
        // I'm root = no siblings
        return NULL;
    }

    int position = m_parent->getChildPosition(this, ordering_index);

    if (position < number_up)
    {
        // not enough siblings "above" me
        return NULL;
    }

    return m_parent->getChildAt(position - number_up, ordering_index);
}

GenericTree* GenericTree::nextSibling(int number_down, int ordering_index)
{
    if (!m_parent)
    {
        // I'm root = no siblings
        return NULL;
    }

    int position = m_parent->getChildPosition(this, ordering_index);

    if (position + number_down >= m_parent->childCount())
    {
        // not enough siblings "below" me
        return NULL;
    }

    return m_parent->getChildAt(position + number_down, ordering_index);
}

GenericTree::iterator GenericTree::begin(void)
{
    return m_subnodes->begin();
}

GenericTree::iterator GenericTree::end(void)
{
    return m_subnodes->end();
}

GenericTree::iterator GenericTree::begin(uint ordering)
{
    if ((int)ordering != m_current_ordering_index)
    {
        reorderSubnodes(ordering);
        m_current_ordering_index = ordering;
    }

    return m_ordered_subnodes->begin();
}

GenericTree::iterator GenericTree::end(uint ordering)
{
    return m_ordered_subnodes->end();
}

GenericTree* GenericTree::getParent()
{
    if (m_parent)
        return m_parent;
    return NULL;
}

void GenericTree::setAttribute(uint attribute_position, int value_of_attribute)
{
    // You can use attributes for anything you like. Mythmusic, for example,
    // stores a value for random ordering in the first "column" (0) and a value
    // for "intelligent" (1) ordering in the second column

    while ((uint)m_attributes->size() < (attribute_position + 1))
        m_attributes->push_back(-1);

    (*m_attributes)[attribute_position] = value_of_attribute;
}

int GenericTree::getAttribute(uint which_one) const
{
    if (m_attributes->size() < (int)(which_one + 1))
    {
        LOG(VB_GENERAL, LOG_ALERT, 
                 "asked a GenericTree node for a nonexistent attribute");
        return 0;
    }

    return m_attributes->at(which_one);
}

void GenericTree::reorderSubnodes(int ordering_index)
{
    // The nodes are there, we just want to re-order them according to
    // attribute column defined by ordering_index

    m_ordered_subnodes->SetSortType(0);
    m_ordered_subnodes->SetOrderingIndex(ordering_index);

    m_ordered_subnodes->sort();
}

void GenericTree::addYourselfIfSelectable(vector<GenericTree*> *flat_list)
{
    if (m_selectable)
        flat_list->push_back(this);

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
        (*it)->addYourselfIfSelectable(flat_list);
}

void GenericTree::buildFlatListOfSubnodes(int ordering_index,
                                          bool scrambled_parents)
{
    // This builds a flat list of every selectable child according to some
    // some_ordering index.

    m_flatened_subnodes->clear();

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
        (*it)->addYourselfIfSelectable(m_flatened_subnodes);

    if (scrambled_parents)
    {
        // sort the flatened list in the order indicated by ordering_index
        m_flatened_subnodes->SetSortType(0);
        m_flatened_subnodes->SetOrderingIndex(ordering_index);
        m_flatened_subnodes->sort();
    }
}

GenericTree* GenericTree::nextPrevFromFlatList(bool forward_or_backward,
                                               bool wrap_around,
                                               GenericTree *active)
{
    vector<GenericTree*>::iterator it =
        std::find(m_flatened_subnodes->begin(),
                  m_flatened_subnodes->end(), active);

    if (it == m_flatened_subnodes->end())
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "Can't find active item on flattened list");
        return NULL;
    }
    int i = it - m_flatened_subnodes->begin();

    if (forward_or_backward)
    {
        ++i;
        if (i >= (int)m_flatened_subnodes->size())
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
                i = m_flatened_subnodes->size() - 1;
            else
                return NULL;
        }
    }

    return m_flatened_subnodes->at(i);
}

GenericTree* GenericTree::getChildByName(const QString &a_name)
{
    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
    {
        if ((*it)->getString() == a_name)
            return (*it);
    }
    return NULL;
}

GenericTree* GenericTree::getChildByInt(int an_int)
{
    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
    {
        if ((*it)->getInt() == an_int)
            return (*it);
    }
    return NULL;
}



void GenericTree::sortByString()
{
    m_ordered_subnodes->SetSortType(1);
    m_ordered_subnodes->sort();

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
        (*it)->sortByString();
}

void GenericTree::sortByAttributeThenByString(int which_attribute)
{
    m_ordered_subnodes->SetSortType(3);
    m_ordered_subnodes->SetOrderingIndex(which_attribute);
    m_ordered_subnodes->sort();

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
        (*it)->sortByAttributeThenByString(which_attribute);
}

void GenericTree::sortBySelectable()
{
    m_ordered_subnodes->SetSortType(2);
    m_ordered_subnodes->sort();

    vector<GenericTree*>::iterator it = m_subnodes->begin();
    for (; it != m_subnodes->end(); ++it)
        (*it)->sortBySelectable();
}

void GenericTree::deleteAllChildren(bool actually_delete)
{
    m_flatened_subnodes->clear();
    m_ordered_subnodes->clear();
    m_selected_subnode = NULL;
    m_current_ordering_index = -1;

    if (actually_delete)
    {
        while (!m_subnodes->empty())
        {
            delete m_subnodes->back();
            m_subnodes->pop_back();
        }
    }
    else
    {
        m_subnodes->clear();
    }
}

void GenericTree::pruneAllChildren()
{
    //
    //  This is just calls deleteAllChildren(), except that we turn off
    //  AutoDelete'ion first. That way, we can pull out children that we
    //  don't "own".
    //

    deleteAllChildren(false); // don't delete the children here...
}

void GenericTree::reOrderAsSorted()
{
    //
    //  Arrange (recursively) my subnodes in the same order as my ordered
    //  subnodes
    //

    if (m_subnodes->size() != m_ordered_subnodes->size())
    {
        LOG(VB_GENERAL, LOG_ALERT, 
                 "Can't reOrderAsSorted(), because the number of subnodes is "
                 "different than the number of ordered subnodes");
        return;
    }

    m_subnodes->clear(); // don't delete the children here...
    m_current_ordering_index = -1;

    vector<GenericTree*>::iterator it = m_ordered_subnodes->begin();
    for (; it != m_ordered_subnodes->end(); ++it)
    {
        m_subnodes->push_back(*it);
        (*it)->reOrderAsSorted();
    }
}

void GenericTree::MoveItemUpDown(GenericTree *item, bool flag)
{
    if (item == m_subnodes->front() && flag)
        return;
    if (item == m_subnodes->back() && !flag)
        return;

    vector<GenericTree*>::iterator it =
        std::find(m_subnodes->begin(), m_subnodes->end(), item);

    if (it == m_subnodes->end())
        return;

    int num = it - m_subnodes->begin();
    int insertat = (flag) ? num - 1 : num + 1;

    m_subnodes->erase(it);
    m_subnodes->insert(m_subnodes->begin() + insertat, item);
}
