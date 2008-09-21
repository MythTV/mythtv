// C/C++ Headers
#include <iostream>

// MythDB headers
#include "mythverbose.h"

// Mythui Headers
#include "mythuibuttontree.h"
#include "mythmainwindow.h"

MythUIButtonTree::MythUIButtonTree(MythUIType *parent, const QString &name)
                 : MythUIType(parent, name)
{
    m_initialized = false;

    m_numLists = 1;
    m_visibleLists = 0;
    m_currentDepth = m_oldDepth = 0;
    m_rootNode = m_currentNode = NULL;
    m_listSpacing = 0;
    m_activeList = NULL;
    m_activeListID = 0;

    m_active = true;

    m_listTemplate = NULL;
    SetCanTakeFocus(true);
}

MythUIButtonTree::~MythUIButtonTree()
{
}

void MythUIButtonTree::Init()
{
    if (!m_listTemplate)
        m_listTemplate = dynamic_cast<MythUIButtonList *>
                                                (GetChild("listtemplate"));

    if (!m_listTemplate)
    {
        VERBOSE(VB_IMPORTANT, QString("MythUIButtonList listtemplate is "
                                      "required in mythuibuttonlist: %1")
                                      .arg(objectName()));
        return;
    }

    m_listTemplate->SetVisible(false);

    int width = (m_Area.width() - (m_listSpacing * (m_numLists-1))) / m_numLists;
    int height = m_Area.height();

    int i = 0;
    while ( i < (int)m_numLists )
    {
        QString listname = QString("buttontree list %1").arg(i);
        MythUIButtonList *list = new MythUIButtonList(this, listname);
        list->CopyFrom(m_listTemplate);
        list->SetVisible(false);
        list->SetActive(false);
        list->SetCanTakeFocus(false);
        int x = i * (width + m_listSpacing);
        MythRect listArea = MythRect(x,0,width,height);
        list->SetArea(listArea);
        m_buttonlists.append(list);
        i++;
    }

    m_initialized = true;
}

void MythUIButtonTree::SetTreeState()
{
    if (!m_initialized)
        Init();

    if (!m_rootNode)
        return;

    if (!m_currentNode)
        SetCurrentNode(m_rootNode);

    MythGenericTree *node = m_rootNode;

    bool refreshAll = false;
    if (m_currentDepth > 0)
    {
        QList<MythGenericTree*> route = m_currentNode->getRoute();
        if ((int)m_currentDepth > route.size())
        {
            VERBOSE(VB_IMPORTANT, "Requested depth is greater than currentNode");
            return;
        }
        node = route.at(m_currentDepth);
        if (m_currentDepth != m_oldDepth)
            refreshAll = true;
    }

    m_oldDepth = m_currentDepth;

    m_visibleLists = 0;
    uint listid = 0;

    while ( listid < m_numLists )
    {
        MythUIButtonList *list = m_buttonlists.at(listid);

        list->SetVisible(false);
        list->SetActive(false);

        MythGenericTree *selectedNode = NULL;

        if (node)
            selectedNode = node->getSelectedChild(true);

        if (refreshAll || m_activeListID <= listid)
        {
            if (!UpdateList(list, node))
                break;

            if (m_active && (listid == m_activeListID))
            {
                m_activeList = list;
                list->SetActive(true);
                emit itemSelected(list->GetItemCurrent());
                SetCurrentNode(selectedNode);
            }
        }

        listid++;

        list->SetVisible(true);
        m_visibleLists++;

        node = selectedNode;
    }
}

bool MythUIButtonTree::UpdateList(MythUIButtonList *list, MythGenericTree *node)
{
    disconnect(list, 0, 0, 0);

    list->Reset();

    QList<MythGenericTree*> *nodelist = NULL;

    if (node)
        nodelist = node->getAllChildren();

    if (!nodelist || nodelist->isEmpty())
        return false;

    MythGenericTree *selectedNode = node->getSelectedChild(true);

    MythUIButtonListItem *selectedItem = NULL;
    QList<MythGenericTree*>::iterator it;
    for (it = nodelist->begin(); it != nodelist->end(); ++it)
    {
        MythGenericTree *childnode = *it;
        if (!childnode->IsVisible())
            continue;

        MythUIButtonListItem *item = new MythUIButtonListItem(list,
                                                        childnode->getString());
        item->SetData(qVariantFromValue(childnode));

        if (childnode->childCount() > 0)
            item->setDrawArrow(true);

        if (childnode == selectedNode)
            selectedItem = item;
    }

    if (selectedItem)
        list->SetItemCurrent(selectedItem);

    connect(list, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(handleSelect(MythUIButtonListItem *)));
    connect(list, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(handleClick(MythUIButtonListItem *)));

    return true;
}

bool MythUIButtonTree::AssignTree(MythGenericTree *tree)
{
    if (!tree)
        return false;

    if (m_rootNode)
        Reset();

    m_rootNode = tree;
    m_currentNode = m_rootNode;
    SetTreeState();

    return true;
}

void MythUIButtonTree::Reset(void)
{
    m_rootNode = m_currentNode = NULL;
    m_visibleLists = 0;
    m_currentDepth = m_oldDepth = 0;
    m_activeList = NULL;
    m_activeListID = 0;
    m_active = true;

    SetTreeState();

    MythUIType::Reset();
}

bool MythUIButtonTree::SetNodeById(QList<int> route)
{
    MythGenericTree *node = m_rootNode->findNode(route);
    if (node && node->isSelectable())
    {
        SetCurrentNode(node);
        SetTreeState();
        return true;
    }
    return false;
}

bool MythUIButtonTree::SetNodeByString(QStringList route)
{
    if (!m_rootNode)
    {
        SetCurrentNode(NULL);
        return false;
    }

    SetCurrentNode(m_rootNode);

    bool foundit = false;
    if (!route.isEmpty())
    {
        if (route[0] == m_rootNode->getString())
        {
            if (route.size() > 1)
            {
                for(int i = 1; i < route.size(); i ++)
                {
                    MythGenericTree *node = m_currentNode->getChildByName(route[i]);
                    if (node)
                    {
                        SetCurrentNode(node);
                        foundit = true;
                    }
                    else
                        break;
                }
            }
            else
                foundit = true;
        }
    }

    SetTreeState();

    return foundit;
}

bool MythUIButtonTree::SetCurrentNode(MythGenericTree *node)
{
    if (node)
    {
        if (node == m_currentNode)
            return true;

        m_currentNode = node;
        node->becomeSelectedChild();
        emit nodeChanged(m_currentNode);
        return true;
        // Ensure that this node exists in the current tree
        //QList<int> route = node->getRouteById();
        //if (SetNodeById(route))
        //    return true;
    }

    return false;
}

void MythUIButtonTree::SetActive(bool active)
{
    m_active = active;
    if (m_initialized)
        SetTreeState();
}

void MythUIButtonTree::SwitchList(bool right)
{
    bool doUpdate = false;
    if (right)
    {
        if (m_activeListID < m_visibleLists-1)
            m_activeListID++;
        else if (m_currentNode->childCount() > 0)
        {
            m_currentDepth++;
            doUpdate = true;
        }
        else
            return;
    }
    else if (!right)
    {
        if (m_activeListID > 0)
            m_activeListID--;
        else if (m_currentDepth > 0)
        {
            m_currentDepth--;
            doUpdate = true;
        }
        else
            return;
    }

    if (doUpdate)
        SetTreeState();
    else
    {
        if (m_activeList)
            m_activeList->Deselect();
        m_activeList = m_buttonlists[m_activeListID];
        m_activeList->Select();
    }
}

void MythUIButtonTree::handleSelect(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythUIButtonList *list = item->parent();
    QString name = list->objectName();

    if (m_activeList)
        m_activeList->Deselect();
    m_activeListID = name.section(' ',2,2).toInt();
    m_activeList = list;

    MythGenericTree *node = qVariantValue<MythGenericTree*> (item->GetData());
    SetCurrentNode(node);
    SetTreeState();
}

void MythUIButtonTree::handleClick(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythGenericTree *node = qVariantValue<MythGenericTree*> (item->GetData());
    if (SetCurrentNode(node))
        emit itemClicked(item);
}

MythUIButtonListItem* MythUIButtonTree::GetItemCurrent() const
{
    if (m_activeList)
        return m_activeList->GetItemCurrent();

    return NULL;
}

bool MythUIButtonTree::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "RIGHT")
        {
            SwitchList(true);
        }
        else if (action == "LEFT")
        {
            SwitchList(false);
        }
        else
            handled = false;
    }

    if (!handled && m_activeList)
        handled = m_activeList->keyPressEvent(event);

    return handled;
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
// void MythUIButtonTree::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
// {
//     if (event->gesture() == MythGestureEvent::Click)
//     {
//         QPoint position = event->GetPosition();
//
//         MythUIType *object = GetChildAt(position, false);
//         object->gestureEvent(object, event);
//     }
// }

bool MythUIButtonTree::ParseElement(QDomElement &element)
{
    if (element.tagName() == "spacing")
    {
        m_listSpacing = NormX(getFirstText(element).toInt());
    }
    if (element.tagName() == "numlists")
    {
        m_numLists = getFirstText(element).toInt();
    }
    else
        return MythUIType::ParseElement(element);

    return true;
}

void MythUIButtonTree::CreateCopy(MythUIType *parent)
{
    MythUIButtonTree *bt = new MythUIButtonTree(parent, objectName());
    bt->CopyFrom(this);
}

void MythUIButtonTree::CopyFrom(MythUIType *base)
{
    MythUIButtonTree *bt = dynamic_cast<MythUIButtonTree *>(base);
    if (!bt)
        return;

    m_numLists = bt->m_numLists;
    m_listSpacing = bt->m_listSpacing;
    m_active = bt->m_active;

    MythUIType::CopyFrom(base);

    m_listTemplate = dynamic_cast<MythUIButtonList *>(GetChild("listtemplate"));

    m_initialized = false;
}
