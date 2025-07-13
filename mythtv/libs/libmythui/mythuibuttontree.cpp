// Own header
#include "mythuibuttontree.h"

// C++
#include <algorithm>
#include <utility>

// Qt headers
#include <QDomDocument>

// Mythbase headers
#include "libmythbase/mythlogging.h"

// Mythui Headers
#include "mythmainwindow.h"
#include "mythgesture.h"

MythUIButtonTree::MythUIButtonTree(MythUIType *parent, const QString &name)
    : MythUIType(parent, name)
{
    SetCanTakeFocus(true);

    connect(this, &MythUIType::TakingFocus, this, &MythUIButtonTree::Select);
    connect(this, &MythUIType::LosingFocus, this, &MythUIButtonTree::Deselect);
}

/*!
 * \brief Initialise the tree having loaded the formatting options from the
 *        theme
 */
void MythUIButtonTree::Init()
{
    if (!m_listTemplate)
        m_listTemplate = dynamic_cast<MythUIButtonList *>
                         (GetChild("listtemplate"));

    if (!m_listTemplate)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("(%1) MythUIButtonList listtemplate "
                                         "is required in mythuibuttonlist: %2")
            .arg(GetXMLLocation().arg(objectName())));
        return;
    }

    m_listTemplate->SetVisible(false);

    int width = (m_area.width() - (m_listSpacing * (m_numLists - 1))) / m_numLists;
    int height = m_area.height();

    int i = 0;

    while (i < (int)m_numLists)
    {
        QString listname = QString("buttontree list %1").arg(i);
        auto *list = new MythUIButtonList(this, listname);
        list->CopyFrom(m_listTemplate);
        list->SetVisible(false);
        list->SetActive(false);
        list->SetCanTakeFocus(false);
        int x = i * (width + m_listSpacing);
        MythRect listArea = MythRect(x, 0, width, height);
        list->SetArea(listArea);
        m_buttonlists.append(list);
        i++;
    }

    m_initialized = true;
}

/*!
 * \brief Update the widget following a change
 */
void MythUIButtonTree::SetTreeState(bool refreshAll)
{
    if (!m_initialized)
        Init();

    if (!m_rootNode)
        return;

    if (!m_currentNode)
        DoSetCurrentNode(m_rootNode->getSelectedChild());
    if (!m_currentNode)
        return;

    QList<MythGenericTree *> route = m_currentNode->getRoute();

    // Sanity Checks
    if (m_depthOffset >= route.size())
        m_depthOffset = 0;

    if (((int)m_currentDepth + m_depthOffset) >= route.size())
        m_currentDepth = 0;

    MythGenericTree *node = route.at(m_currentDepth + m_depthOffset);

    if (m_currentDepth != m_oldDepth)
        refreshAll = true;

    m_oldDepth = m_currentDepth;

    m_visibleLists = 0;
    uint listid = 0;

    while (listid < (uint)m_buttonlists.count())
    {
        MythUIButtonList *list = m_buttonlists.at(listid);

        list->SetVisible(false);
        list->SetActive(false);

        MythGenericTree *selectedNode = nullptr;

        if (node)
            selectedNode = node->getSelectedChild(true);

        if (refreshAll || m_activeListID < listid)
        {
            if (!UpdateList(list, node))
            {
                listid++;
                continue;
            }
        }

        if (m_active && (listid == m_activeListID))
        {
            m_activeList = list;
            list->SetActive(true);
            DoSetCurrentNode(selectedNode);
            emit itemSelected(list->GetItemCurrent());
        }

        listid++;

        list->SetVisible(true);
        m_visibleLists++;

        node = selectedNode;
    }
}

/*!
 * \brief Update a list with children of the tree node
 *
 * \param list List to refill
 * \param node Parent node whose children will appear in the list
 *
 * \return True if successful, False if the node had no children or was invalid
 */
bool MythUIButtonTree::UpdateList(MythUIButtonList *list, MythGenericTree *node) const
{
    disconnect(list, nullptr, nullptr, nullptr);

    list->Reset();

    QList<MythGenericTree *> *nodelist = nullptr;

    if (node)
        nodelist = node->getAllChildren();

    if (!nodelist || nodelist->isEmpty())
        return false;

    MythGenericTree *selectedNode = node->getSelectedChild(true);

    MythUIButtonListItem *selectedItem = nullptr;
    QList<MythGenericTree *>::iterator it;

    for (it = nodelist->begin(); it != nodelist->end(); ++it)
    {
        MythGenericTree *childnode = *it;

        if (!childnode->IsVisible())
            continue;

        MythUIButtonListItem *item = childnode->CreateListButton(list);

        if (childnode == selectedNode)
            selectedItem = item;
    }

    if (list->IsEmpty())
        return false;

    if (selectedItem)
        list->SetItemCurrent(selectedItem);

    connect(list, &MythUIButtonList::itemSelected,
            this, &MythUIButtonTree::handleSelect);
    connect(list, &MythUIButtonList::itemClicked,
            this, &MythUIButtonTree::handleClick);
    connect(list, &MythUIButtonList::itemVisible,
            this, &MythUIButtonTree::handleVisible);

    return true;
}

/*!
 * \brief Assign the root node of the tree to be displayed
 *
 * MythUIButtonTree() is merely responsible for the display and navigation of a
 * tree, the structure of that tree and the data it contains are the
 * responsibility of MythGenericTree(). Generally MythUIButtonTree() will not
 * modify the tree, for technical reason RemoteItem() is an exception to the rule.
 *
 * You should operate directly on MythGenericTree() to change it's content.
 *
 * \param tree The node to make the root of the displayed tree
 *
 * \return True if successful
 */
bool MythUIButtonTree::AssignTree(MythGenericTree *tree)
{
    if (!tree || !tree->visibleChildCount())
        return false;

    if (m_rootNode)
        Reset();

    m_rootNode = tree;
    m_currentNode = m_rootNode->getSelectedChild();
    // The node we are given may not be the root node of that tree, we need
    // to keep track of our depth in the tree so that we can navigate
    // as though the parent nodes do not exist
    m_depthOffset = m_rootNode->currentDepth();
    SetTreeState(true);
    emit rootChanged(m_rootNode);

    return true;
}

/*!
 * \copydoc MythUIType::Reset()
 */
void MythUIButtonTree::Reset(void)
{
    m_rootNode = m_currentNode = nullptr;
    m_visibleLists = 0;
    m_currentDepth = m_oldDepth = 0;
    m_activeList = nullptr;
    m_activeListID = 0;
    m_active = true;

    SetTreeState(true);
    emit rootChanged(m_rootNode);

    MythUIType::Reset();
}

/*!
 * \brief Using a path based on the node IDs, set the currently
 *        selected node
 *
 * \param route List defining the path using node IDs starting at the root node
 *
 * \return True if successful
 */
bool MythUIButtonTree::SetNodeById(QList<int> route)
{
    MythGenericTree *node = m_rootNode->findNode(std::move(route));

    if (node && node->isSelectable())
    {
        DoSetCurrentNode(node);
        SetTreeState();
        return true;
    }

    return false;
}

/*!
 * \brief Using a path based on the node string, set the currently
 *        selected node
 *
 * \param route List defining the path using node strings starting at the root node
 *
 * \return True if successful
 */
bool MythUIButtonTree::SetNodeByString(QStringList route)
{
    if (!m_rootNode)
    {
        DoSetCurrentNode(nullptr);
        return false;
    }

    MythGenericTree *foundNode = m_rootNode;

    bool foundit = false;

    if (!route.isEmpty())
    {
        if (route[0] == m_rootNode->GetText())
        {
            if (route.size() > 1)
            {
                for (int i = 1; i < route.size(); i ++)
                {
                    MythGenericTree *node = foundNode->getChildByName(route[i]);

                    if (node)
                    {
                        node->becomeSelectedChild();
                        foundNode = node;
                        foundit = true;
                    }
                    else
                    {
                        node = foundNode->getChildAt(0);

                        if (node)
                        {
                            node->becomeSelectedChild();
                            foundNode = node;
                        }

                        break;
                    }
                }
            }
            else
            {
                foundit = true;
            }
        }
    }

    DoSetCurrentNode(foundNode);

    m_currentDepth = std::max(0, (int)(foundNode->currentDepth() - m_depthOffset - m_numLists));
    m_activeListID = std::min(foundNode->currentDepth() - m_depthOffset - 1, (int)(m_numLists - 1));

    SetTreeState(true);

    return foundit;
}

/*!
 * \brief Set the currently selected node
 *
 * \param node The node, which must appear in this tree or behaviour is
 *             undefined
 *
 * \return True if successful
 */
bool MythUIButtonTree::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return false;

    if (node == m_currentNode)
        return true;

    QStringList route = node->getRouteByString();

    return SetNodeByString(route);
}

void MythUIButtonTree::ShowSearchDialog(void)
{
    if (m_activeList)
        m_activeList->ShowSearchDialog();
}

bool MythUIButtonTree::DoSetCurrentNode(MythGenericTree *node)
{
    if (node)
    {
        if (node == m_currentNode)
            return true;

        m_currentNode = node;
        node->becomeSelectedChild();
        emit nodeChanged(m_currentNode);
        return true;
    }

    return false;
}

/*!
 * \brief Remove the item from the tree
 *
 * \param item Item to be removed
 * \param deleteNode Also delete the node from the tree? Modifies the tree.
 *
 * \return True if successful
 */
void MythUIButtonTree::RemoveItem(MythUIButtonListItem *item, bool deleteNode)
{
    if (!item || !m_rootNode)
        return;

    auto *node = item->GetData().value<MythGenericTree *>();

    if (node && node->getParent())
    {
        DoSetCurrentNode(node->getParent());

        if (deleteNode)
            node->getParent()->deleteNode(node);
        else
            node->SetVisible(false);
    }

    MythUIButtonList *list = item->parent();

    list->RemoveItem(item);

    if (list->IsEmpty())
    {
        if (m_currentDepth > 0)
            m_currentDepth--;
        else if (m_activeListID > 1)
            m_activeListID--;

        SetTreeState(true);
    }
}

/*!
 * \brief Remove the currently selected item from the tree
 *
 * \param deleteNode Also delete the node from the tree? Modifies the tree.
 *
 * \return True if successful
 */
void MythUIButtonTree::RemoveCurrentItem(bool deleteNode)
{
    RemoveItem(GetItemCurrent(), deleteNode);
}

/*!
 * \brief Set the widget active/inactive
 *
 * \param active
 */
void MythUIButtonTree::SetActive(bool active)
{
    m_active = active;

    if (m_initialized)
        SetTreeState();
}

void MythUIButtonTree::Select()
{
    SetActive(true);
}

void MythUIButtonTree::Deselect()
{
    SetActive(false);
}


/*!
 * \brief Move from list, or one level of the tree, to another
 *
 * \param right If true move to the right or away from the root, if false move to
 *              the left or towards the root
 */
void MythUIButtonTree::SwitchList(bool right)
{
    bool doUpdate = false;

    if (right)
    {
        if ((m_activeListID + 1 < m_visibleLists) &&
            (m_activeListID + 1 < (uint)m_buttonlists.count()))
            m_activeListID++;
        else if (m_currentNode && m_currentNode->visibleChildCount() > 0)
        {
            ++m_currentDepth;
            doUpdate = true;
        }
        else
        {
            return;
        }
    }
    else if (!right)
    {
        if (m_activeListID > 0)
            --m_activeListID;
        else if (m_currentDepth > 0)
        {
            --m_currentDepth;
            doUpdate = true;
        }
        else
        {
            return;
        }
    }

    if (doUpdate)
        SetTreeState();
    else
    {
        if (m_activeList)
            m_activeList->Deselect();

        if (m_activeListID < (uint)m_buttonlists.count())
        {
            m_activeList = m_buttonlists[m_activeListID];
            m_activeList->Select();
        }
    }
}

/*!
 * \brief Handle a list item receiving focus
 *
 * \param item The list item
 */
void MythUIButtonTree::handleSelect(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythUIButtonList *list = item->parent();
    QString name = list->objectName();

    // New list is automatically selected so we just need to deselect the old
    // list
    if (m_activeList)
        m_activeList->Deselect();

    m_activeListID = name.section(' ', 2, 2).toInt();
    m_activeList = list;

    auto *node = item->GetData().value<MythGenericTree *> ();
    DoSetCurrentNode(node);
    SetTreeState();
}

/*!
 * \brief Handle a list item being clicked
 *
 * \param item The list item
 */
void MythUIButtonTree::handleClick(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *node = item->GetData().value<MythGenericTree *>();

    if (DoSetCurrentNode(node))
        emit itemClicked(item);
}

/*!
 * \brief Return the currently selected list item
 *
 * \return The list item
 */
MythUIButtonListItem *MythUIButtonTree::GetItemCurrent() const
{
    if (m_activeList)
        return m_activeList->GetItemCurrent();

    return nullptr;
}

/*!
* \brief Handle a list item becoming visible
*
* \param item The list item
*/
void MythUIButtonTree::handleVisible(MythUIButtonListItem *item)
{
    if (!item)
        return;

    emit itemVisible(item);
}

/*!
 * \copydoc MythUIType::keyPressEvent()
 */
bool MythUIButtonTree::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (m_activeList && m_activeList->m_layout == MythUIButtonList::LayoutGrid)
        {
            if (action == "SELECT" && m_currentNode->visibleChildCount() > 0)
            {
                SwitchList(true);
            }
            else if (action == "ESCAPE" && m_currentDepth > 0)
            {
                SwitchList(false);
            }
            else
            {
                handled = false;
            }
        }
        else
        {
            if (action == "RIGHT" && m_currentNode->visibleChildCount() > 0)
            {
                SwitchList(true);
            }
            else if (action == "LEFT" && (m_currentDepth != 0 || m_activeListID != 0))
            {
                SwitchList(false);
            }
            else
            {
                handled = false;
            }
        }
    }

    if (!handled && m_activeList)
        handled = m_activeList->keyPressEvent(event);

    return handled;
}

bool MythUIButtonTree::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;

    if (event->GetGesture() == MythGestureEvent::Click)
    {
        // We want the relative position of the click
        QPoint position = event->GetPosition() -
                          m_parent->GetArea().topLeft();

        MythUIType *type = GetChildAt(position, false, false);

        if (!type)
            return false;

        auto *list = dynamic_cast<MythUIButtonList *>(type);

        if (list)
            handled = list->gestureEvent(event);
    }

    return handled;
}

/*!
 * \copydoc MythUIType::ParseElement()
 */
bool MythUIButtonTree::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "spacing")
    {
        m_listSpacing = NormX(getFirstText(element).toInt());
    }
    else if (element.tagName() == "numlists")
    {
        m_numLists = getFirstText(element).toInt();
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/*!
 * \copydoc MythUIType::CreateCopy()
 */
void MythUIButtonTree::CreateCopy(MythUIType *parent)
{
    auto *bt = new MythUIButtonTree(parent, objectName());
    bt->CopyFrom(this);
}

/*!
 * \copydoc MythUIType::CopyFrom()
 */
void MythUIButtonTree::CopyFrom(MythUIType *base)
{
    auto *bt = dynamic_cast<MythUIButtonTree *>(base);

    if (!bt)
        return;

    m_numLists = bt->m_numLists;
    m_listSpacing = bt->m_listSpacing;
    m_active = bt->m_active;

    MythUIType::CopyFrom(base);

    m_listTemplate = dynamic_cast<MythUIButtonList *>(GetChild("listtemplate"));

    m_initialized = false;
}
