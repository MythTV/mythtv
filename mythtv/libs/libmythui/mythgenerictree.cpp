
// Mythui headers
#include "mythgenerictree.h"
#include "mythuibuttonlist.h"

// Myth headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsorthelper.h"

// QT headers
#include <algorithm>

class SortableMythGenericTreeList : public QList<MythGenericTree*>
{
  public:
    SortableMythGenericTreeList() = default;
    enum SortType : std::uint8_t {SORT_STRING=0, SORT_SELECTABLE=1};

    void SetSortType(SortType stype) { m_sortType = stype; }
    void SetAttributeIndex(int index)
                                { m_attributeIndex = (index >= 0) ? index : 0; }

    static bool sortByString(MythGenericTree *one, MythGenericTree *two)
    {
        return one->GetSortText() < two->GetSortText();
    }

    static int sortBySelectable(MythGenericTree *one, MythGenericTree *two)
    {
        bool onesel = one->isSelectable();
        bool twosel = two->isSelectable();

        if (onesel == twosel)
            return 0;
        if (onesel && !twosel)
            return 1;
        return -1;
    }

    void Sort(SortType stype, int attributeIndex = 0)
    {
        m_sortType = stype;
        m_attributeIndex = attributeIndex;
        switch (m_sortType)
        {
            case SORT_STRING:
                std::sort(begin(), end(), sortByString);
                break;
            case SORT_SELECTABLE:
                std::sort(begin(), end(), sortBySelectable);
                break;
        }
    }

  private:
    SortType m_sortType  {SORT_STRING};
    int m_attributeIndex {0}; // for getAttribute
};

///////////////////////////////////////////////////////

MythGenericTree::MythGenericTree(QString a_string, int an_int,
                         bool selectable_flag)
    : m_text(std::move(a_string)),
      m_int(an_int),
      m_subnodes(new SortableMythGenericTreeList),
      m_selectable(selectable_flag)
{
    ensureSortFields();
}

MythGenericTree::~MythGenericTree()
{
    deleteAllChildren();
    delete m_subnodes;
}

void MythGenericTree::ensureSortFields(void)
{
    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
    if (m_sortText.isEmpty() and not m_text.isEmpty())
        m_sortText = sh->doTitle(m_text);
}

MythGenericTree* MythGenericTree::addNode(const QString &a_string, int an_int,
                                  bool selectable_flag, bool visible)
{
    auto *new_node = new MythGenericTree(a_string.simplified(),
                                                    an_int, selectable_flag);
    new_node->SetVisible(visible);
    return addNode(new_node);
}

MythGenericTree* MythGenericTree::addNode(const QString &a_string,
                                  const QString &sortText, int an_int, bool
                                  selectable_flag, bool visible)
{
    auto *new_node = new MythGenericTree(a_string.simplified(),
                                                    an_int, selectable_flag);
    new_node->SetVisible(visible);
    new_node->SetSortText(sortText);

    return addNode(new_node);
}

MythGenericTree *MythGenericTree::addNode(MythGenericTree *child)
{
    child->setParent(this);
    m_subnodes->append(child);
    if (child->IsVisible())
        IncVisibleCount();

    return child;
}

void MythGenericTree::DetachParent(void)
{
    if (!m_parent)
        return;

    m_parent->removeNode(this);
}

void MythGenericTree::removeNode(MythGenericTree *child)
{
    if (!child)
        return;

    if (m_selectedSubnode == child)
        m_selectedSubnode = nullptr;

    m_subnodes->removeAll(child);
    child->setParent(nullptr);

    if (child && child->IsVisible())
        DecVisibleCount();
}

void MythGenericTree::deleteNode(MythGenericTree *child)
{
    if (!child)
        return;

    removeNode(child);
    delete child;
}

MythGenericTree* MythGenericTree::findLeaf()
{
    if (m_subnodes->count() > 0)
        return m_subnodes->first()->findLeaf();

    return this;
}

MythGenericTree* MythGenericTree::findNode(QList<int> route_of_branches)
{
    // Starting from *this* node (which will often be root) find a set of
    // branches that have id's that match the collection passed in
    // route_of_branches. Return the end point of those branches.
    //
    // In practical terms, mythmusic will use this to force the playback
    // screen's ManagedTreeList to move to a given track in a given playlist

    MythGenericTree *node = nullptr;
    for (int i = 0; i < route_of_branches.count(); i++)
    {
        if (!node)
            node = this;

        bool foundit = false;
        QList<MythGenericTree*>::iterator it;
        QList<MythGenericTree*> *children = node->getAllChildren();

        if (!children)
            break;

        MythGenericTree *child = nullptr;

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

    return nullptr;
}

int MythGenericTree::getChildPosition(MythGenericTree *child) const
{
    return m_subnodes->indexOf(child);
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

    routeByString.push_front(GetText());

    MythGenericTree *parent = this;
    while( (parent = parent->getParent()) )
    {
        routeByString.push_front(parent->GetText());
    }
    return routeByString;
}

QList<MythGenericTree*> MythGenericTree::getRoute(void)
{
    QList<MythGenericTree*> route;

    route.push_front(this);

    MythGenericTree *parent = this;
    while( (parent = parent->getParent()) )
    {
        route.push_front(parent);
    }
    return route;
}

int MythGenericTree::childCount(void) const
{
    return m_subnodes->count();
}

int MythGenericTree::siblingCount(void) const
{
    if (m_parent)
        return m_parent->childCount();
    return 1;
}

/**
 * \brief Establish how deep in the current tree this node lies
 */
int MythGenericTree::currentDepth(void)
{
    QList<MythGenericTree *> route = getRoute();

    return (route.size() - 1);
}

QList<MythGenericTree*> *MythGenericTree::getAllChildren() const
{
    return m_subnodes;
}

MythGenericTree* MythGenericTree::getChildAt(uint reference) const
{
    if (reference >= (uint)m_subnodes->count())
        return nullptr;

    return m_subnodes->at(reference);
}

MythGenericTree* MythGenericTree::getVisibleChildAt(uint reference) const
{
    if (reference >= (uint)m_subnodes->count())
        return nullptr;

    QList<MythGenericTree*> *list = m_subnodes;

    uint n = 0;
    for (auto *child : std::as_const(*list))
    {
        if (child->IsVisible())
        {
            if (n == reference)
                return child;
            n++;
        }
    }

    return nullptr;
}

MythGenericTree* MythGenericTree::getSelectedChild(bool onlyVisible) const
{
    MythGenericTree *selectedChild = nullptr;

    if (m_selectedSubnode)
        selectedChild = m_selectedSubnode;
    else if (onlyVisible)
        selectedChild = getVisibleChildAt(0);
    else
        selectedChild = getChildAt(0);

    return selectedChild;
}

void MythGenericTree::becomeSelectedChild()
{
    if (m_parent)
        m_parent->setSelectedChild(this);
    else
        LOG(VB_GENERAL, LOG_ERR, "Top level can't become selected child");
}

MythGenericTree* MythGenericTree::prevSibling(int number_up)
{
    if (!m_parent)
    {
        // I'm root = no siblings
        return nullptr;
    }

    int position = m_parent->getChildPosition(this);

    if (position < number_up)
    {
        // not enough siblings "above" me
        return nullptr;
    }

    return m_parent->getChildAt(position - number_up);
}

MythGenericTree* MythGenericTree::nextSibling(int number_down)
{
    if (!m_parent)
    {
        // I'm root = no siblings
        return nullptr;
    }

    int position = m_parent->getChildPosition(this);

    if (position + number_down >= m_parent->childCount())
    {
        // not enough siblings "below" me
        return nullptr;
    }

    return m_parent->getChildAt(position + number_down);
}

MythGenericTree* MythGenericTree::getParent() const
{
    if (m_parent)
        return m_parent;
    return nullptr;
}

MythGenericTree* MythGenericTree::getChildByName(const QString &a_name) const
{
    QList<MythGenericTree*> *children = getAllChildren();
    if (children && children->count() > 0)
    {
        SortableMythGenericTreeList::Iterator it;
        MythGenericTree *child = nullptr;

        for (it = children->begin(); it != children->end(); ++it)
        {
            child = *it;
            if (!child)
                continue;
            if (child->GetText() == a_name)
                return child;
        }
    }

    return nullptr;
}

MythGenericTree* MythGenericTree::getChildById(int an_int) const
{
    QList<MythGenericTree*> *children = getAllChildren();
    if (children && children->count() > 0)
    {
        SortableMythGenericTreeList::Iterator it;
        MythGenericTree *child = nullptr;

        for (it = children->begin(); it != children->end(); ++it)
        {
            child = *it;
            if (!child)
                continue;
            if (child->getInt() == an_int)
                return child;
        }
    }

    return nullptr;
}

void MythGenericTree::sortByString()
{
    m_subnodes->Sort(SortableMythGenericTreeList::SORT_STRING);

    QList<MythGenericTree*> *children = getAllChildren();
    if (children && children->count() > 0)
    {
        SortableMythGenericTreeList::Iterator it;
        MythGenericTree *child = nullptr;

        for (it = children->begin(); it != children->end(); ++it)
        {
            child = *it;
            if (!child)
                continue;
            child->sortByString();
        }
    }
}

void MythGenericTree::sortBySelectable()
{
    m_subnodes->Sort(SortableMythGenericTreeList::SORT_SELECTABLE);

    QList<MythGenericTree*>::iterator it;
    it = m_subnodes->begin();
    MythGenericTree *child = nullptr;
    while ((child = *it) != nullptr)
    {
        child->sortBySelectable();
        ++it;
    }
}

void MythGenericTree::deleteAllChildren()
{
    m_selectedSubnode = nullptr;
    while (!m_subnodes->isEmpty())
    {
        MythGenericTree *child = m_subnodes->takeFirst();
        delete child;
        child = nullptr;
    }
    m_subnodes->clear();
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

void MythGenericTree::SetVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;

    if (!m_parent)
        return;

    if (visible)
        m_parent->IncVisibleCount();
    else
        m_parent->DecVisibleCount();
}

MythUIButtonListItem *MythGenericTree::CreateListButton(MythUIButtonList *list)
{
    auto *item = new MythUIButtonListItem(list, GetText());
    item->SetData(QVariant::fromValue(this));
    item->SetTextFromMap(m_strings);
    item->SetImageFromMap(m_imageFilenames);
    item->SetStatesFromMap(m_states);
    item->SetTextCb(m_textCb.fn, m_textCb.data);
    item->SetImageCb(m_imageCb.fn, m_imageCb.data);
    item->SetStateCb(m_stateCb.fn, m_stateCb.data);

    if (visibleChildCount() > 0)
        item->setDrawArrow(true);

    return item;
}

void MythGenericTree::SetText(const QString &text, const QString &name,
                              const QString &state)
{
    if (!name.isEmpty())
    {
        TextProperties textprop;
        textprop.text = text;
        textprop.state = state;
        m_strings.insert(name, textprop);
    }
    else
    {
        m_text = text;
        m_sortText = nullptr;
        ensureSortFields();
    }
}

void MythGenericTree::SetTextFromMap(const InfoMap &infoMap,
                                     const QString &state)
{
    InfoMap::const_iterator map_it = infoMap.begin();
    while (map_it != infoMap.end())
    {
        TextProperties textprop;
        textprop.text = (*map_it);
        textprop.state = state;
        m_strings[map_it.key()] = textprop;
        ++map_it;
    }
}

void MythGenericTree::SetTextCb(mgtCbFn fn, void *data)
{
    m_textCb.fn = fn;
    m_textCb.data = data;
}

QString MythGenericTree::GetText(const QString &name) const
{
    if (name.isEmpty())
        return m_text;

    if (m_textCb.fn != nullptr)
    {
        QString result = m_textCb.fn(name, m_textCb.data);
        if (!result.isEmpty())
            return result;
    }

    if (m_strings.contains(name))
        return m_strings[name].text;
    return {};
}

void MythGenericTree::SetImage(const QString &filename, const QString &name)
{
    if (!name.isEmpty())
        m_imageFilenames.insert(name, filename);
}

void MythGenericTree::SetImageFromMap(const InfoMap &infoMap)
{
    m_imageFilenames.clear();
    m_imageFilenames = infoMap;
}

void MythGenericTree::SetImageCb(mgtCbFn fn, void *data)
{
    m_imageCb.fn = fn;
    m_imageCb.data = data;
}

QString MythGenericTree::GetImage(const QString &name) const
{
    if (name.isEmpty())
        return {};

    if (m_imageCb.fn != nullptr)
    {
        QString result = m_imageCb.fn(name, m_imageCb.data);
        if (!result.isEmpty())
            return result;
    }

    InfoMap::const_iterator it = m_imageFilenames.find(name);
    if (it != m_imageFilenames.end())
        return *it;

    return {};
}

void MythGenericTree::DisplayStateFromMap(const InfoMap &infoMap)
{
    m_states.clear();
    m_states = infoMap;
}

void MythGenericTree::SetStateCb(mgtCbFn fn, void *data)
{
    m_stateCb.fn = fn;
    m_stateCb.data = data;
}

void MythGenericTree::DisplayState(const QString &state, const QString &name)
{
    if (!name.isEmpty())
        m_states.insert(name, state);
}

QString MythGenericTree::GetState(const QString &name) const
{
    if (name.isEmpty())
        return {};

    if (m_stateCb.fn != nullptr)
    {
        QString result = m_stateCb.fn(name, m_stateCb.data);
        if (!result.isEmpty())
            return result;
    }

    InfoMap::const_iterator it = m_states.find(name);
    if (it != m_states.end())
        return *it;

    return {};
}
