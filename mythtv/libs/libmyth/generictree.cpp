#include <iostream>
using namespace std;

#include "generictree.h"

class SortableGenericTreeList : public QPtrList<GenericTree>
{
  public:
    void SetSortType(int stype) { sort_type = stype; }
    void SetOrderingIndex(int oindex) 
    { ordering_index = (oindex >= 0) ? oindex : 0; }

    int compareItems(QPtrCollection::Item item1, QPtrCollection::Item item2)
    {
        GenericTree *one = (GenericTree *)item1;
        GenericTree *two = (GenericTree *)item2;

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
            QString ones = one->getString().lower();
            QString twos = two->getString().lower();
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
                QString ones = one->getString().lower();
                QString twos = two->getString().lower();
                return QString::localeAwareCompare(ones, twos);
            }
            else if (onea < twoa)
                return -1;
            else
                return 1;
        }
        else
        {
            cerr << "generictree.o: SortableGenericTreeList was asked to "
                 << "compare items (probably inside a sort()), but the "
                 << "sort_type is not set to anything recognizable"
                 << endl;
            return 0;
        }
    }

  private:
    int sort_type; // 0 - getAttribute
                   // 1 - getString
                   // 2 - isSelectable
                   // 3 - attribute, then string
    int ordering_index; // for getAttribute
};

GenericTree::GenericTree(const QString &a_string, int an_int, 
                         bool selectable_flag)
{
    m_subnodes = new SortableGenericTreeList;
    m_ordered_subnodes = new SortableGenericTreeList;
    m_flatened_subnodes = new SortableGenericTreeList;

    m_subnodes->setAutoDelete(true);

    m_parent = NULL;
    m_selected_subnode = NULL;
    m_current_ordering_index = -1;

    // Use 4 here, because we know that's what mythmusic wants (limits resizing)
    m_attributes = new IntVector(4);
    
    m_string = a_string;
    m_int = an_int;
    m_selectable = selectable_flag;
}

GenericTree::~GenericTree()
{
    delete m_subnodes;
    delete m_ordered_subnodes;
    delete m_flatened_subnodes;
    delete m_attributes;
}

GenericTree* GenericTree::addNode(const QString &a_string, int an_int, 
                                  bool selectable_flag)
{
    GenericTree *new_node = new GenericTree(a_string.stripWhiteSpace(),
                                            an_int, selectable_flag);

    return addNode(new_node);
}

GenericTree *GenericTree::addNode(GenericTree *child)
{
    child->setParent(this);
    m_subnodes->append(child);
    m_ordered_subnodes->append(child);

    return child;
}

void GenericTree::removeNode(GenericTree *child)
{
    if (m_selected_subnode == child)
        m_selected_subnode = NULL;

    m_ordered_subnodes->removeRef(child);
    m_flatened_subnodes->removeRef(child);
    m_subnodes->removeRef(child);
}

int GenericTree::calculateDepth(int start)
{
    int current_depth;
    int found_depth;
    current_depth = start + 1;
    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;

    while ((child = it.current()) != 0)
    {
        found_depth = child->calculateDepth(start + 1);
        if (found_depth > current_depth)
            current_depth = found_depth;
        ++it;
    }

    return current_depth;
}

GenericTree* GenericTree::findLeaf(int ordering_index)
{
    if (m_subnodes->count() > 0)
    {
        if (ordering_index == -1)
            return m_subnodes->getFirst()->findLeaf();

        GenericTree *first_child = getChildAt(0, ordering_index);

        return first_child->findLeaf(ordering_index);
    }

    return this;
}

GenericTree* GenericTree::findNode(QValueList<int> route_of_branches)
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

GenericTree* GenericTree::recursiveNodeFinder(QValueList<int> route_of_branches)
{
    if (checkNode(route_of_branches))
        return this;
    else
    {
        QPtrListIterator<GenericTree> it(*m_subnodes);
        GenericTree *child;

        while ((child = it.current()) != 0)
        {
            GenericTree *sub_checker;
            sub_checker = child->recursiveNodeFinder(route_of_branches);
            if (sub_checker)
                return sub_checker;
            else
                ++it;
        }
    }

    return NULL;
}

bool GenericTree::checkNode(QValueList<int> route_of_branches)
{
    bool found_it = true;
    GenericTree *parent_finder = this;

    // FIXME: slow

    for (int i = route_of_branches.count() - 1; i > -1 && found_it; --i)
    {
        if (!(parent_finder->getInt() == (*route_of_branches.at(i))))
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
        return m_subnodes->findRef(child);

    if (m_current_ordering_index != ordering_index)
    {
        reorderSubnodes(ordering_index);
        m_current_ordering_index = ordering_index;
    }

    return m_ordered_subnodes->findRef(child);
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
    return m_subnodes->count();
}

int GenericTree::siblingCount(void)
{
    if (m_parent)
        return m_parent->childCount();
    return 1;
}

QPtrList<GenericTree> *GenericTree::getAllChildren(int ordering_index)
{
    if (ordering_index == -1)
        return m_subnodes;

    return m_ordered_subnodes;
}

GenericTree* GenericTree::getChildAt(uint reference, int ordering_index)
{
    if (reference >= m_ordered_subnodes->count())
    {
        // cerr << "GenericTree: out of bounds request to getChildAt()\n";
        return NULL;
    }

    if (ordering_index == -1)
        return m_subnodes->at(reference);

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
        cerr << "Top level can't become selected child\n";
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

QPtrListIterator<GenericTree> GenericTree::getFirstChildIterator(int ordering)
{
    if (ordering == -1)
        return QPtrListIterator<GenericTree>(*m_subnodes);

    if (ordering != m_current_ordering_index)
    {
        reorderSubnodes(ordering);
        m_current_ordering_index = ordering;
    }

    return QPtrListIterator<GenericTree>(*m_ordered_subnodes);
}

GenericTree* GenericTree::getParent()
{
    if (m_parent)
        return m_parent;
    return NULL;
}

void GenericTree::setAttribute(uint attribute_position, int value_of_attribute)
{
    // You can use attibutes for anything you like. Mythmusic, for example, 
    // stores a value for random ordering in the first "column" (0) and a value
    // for "intelligent" (1) ordering in the second column
    
    if (m_attributes->size() < attribute_position + 1)
        m_attributes->resize(attribute_position + 1, -1);
    m_attributes->at(attribute_position) = value_of_attribute;
}

int GenericTree::getAttribute(uint which_one)
{
    if (m_attributes->size() < which_one + 1)
    {
        cerr << "asked a GenericTree node for a nonexistant attribute\n";
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

void GenericTree::addYourselfIfSelectable(QPtrList<GenericTree> *flat_list)
{
    if (m_selectable)
        flat_list->append(this);

    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        child->addYourselfIfSelectable(flat_list);
        ++it;
    }
}

void GenericTree::buildFlatListOfSubnodes(int ordering_index, 
                                          bool scrambled_parents)
{
    // This builds a flat list of every selectable child according to some 
    // some_ordering index.
    
    m_flatened_subnodes->clear();

    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        child->addYourselfIfSelectable(m_flatened_subnodes);
        ++it;
    }

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
    int i = m_flatened_subnodes->findRef(active);
    if (i < 0)
    {
        cerr << "Can't find active item on flatened list\n";
        return NULL;
    }

    if (forward_or_backward)
    {
        ++i;
        if (i >= (int)m_flatened_subnodes->count())
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
                i = m_flatened_subnodes->count() - 1;
            else
                return NULL;
        }
    }

    return m_flatened_subnodes->at(i);
}

GenericTree* GenericTree::getChildByName(const QString &a_name)
{
    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        if (child->getString() == a_name)
            return child;
        ++it;
    }
    return NULL;
}

GenericTree* GenericTree::getChildByInt(int an_int)
{
    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        if (child->getInt() == an_int)
            return child;
        ++it;
    }
    return NULL;
}



void GenericTree::sortByString()
{
    m_ordered_subnodes->SetSortType(1);
    m_ordered_subnodes->sort();

    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        child->sortByString();
        ++it;
    }
}

void GenericTree::sortByAttributeThenByString(int which_attribute)
{
    m_ordered_subnodes->SetSortType(3);
    m_ordered_subnodes->SetOrderingIndex(which_attribute);
    m_ordered_subnodes->sort();

    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        child->sortByAttributeThenByString(which_attribute);
        ++it;
    }
}

void GenericTree::sortBySelectable()
{
    m_ordered_subnodes->SetSortType(2);
    m_ordered_subnodes->sort();

    QPtrListIterator<GenericTree> it(*m_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        child->sortBySelectable();
        ++it;
    }
}

void GenericTree::deleteAllChildren()
{
    m_flatened_subnodes->clear();
    m_ordered_subnodes->clear();
    m_selected_subnode = NULL;
    m_current_ordering_index = -1;
    m_subnodes->clear();
}

void GenericTree::pruneAllChildren()
{
    //
    //  This is just calls deleteAllChildren(), except that we turn off
    //  AutoDelete'ion first. That way, we can pull out children that we
    //  don't "own".
    //

    m_subnodes->setAutoDelete(false);
    deleteAllChildren();
    m_subnodes->setAutoDelete(true);
}

void GenericTree::reOrderAsSorted()
{
    //
    //  Arrange (recursively) my subnodes in the same order as my ordered
    //  subnodes
    //
    
    if (m_subnodes->count() != m_ordered_subnodes->count())
    {
        cerr << "generictree.o: Can't reOrderAsSorted(), because the number "
             << "of subnodes is different than the number of ordered subnodes"
             << endl;
        return;
    }

    m_subnodes->setAutoDelete(false);
    m_subnodes->clear();    
    m_subnodes->setAutoDelete(true);
    m_current_ordering_index = -1;
    

    QPtrListIterator<GenericTree> it(*m_ordered_subnodes);
    GenericTree *child;
    while ((child = it.current()) != 0)
    {
        m_subnodes->append(child);
        child->reOrderAsSorted();
        ++it;
    }
}

void GenericTree::MoveItemUpDown(GenericTree *item, bool flag)
{
    if (item == m_subnodes->getFirst() && flag)
        return;
    if (item == m_subnodes->getLast() && !flag)
        return;

    int num = m_subnodes->findRef(item);

    int insertat = 0;
    if (flag)
        insertat = num - 1;
    else
        insertat = num + 1;

    m_subnodes->take();
    m_subnodes->insert(insertat, item);
}

