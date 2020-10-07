// Own header
#include "mythuistatetype.h"
#include "mythuibuttonlist.h"

// Qt headers
#include <QDomDocument>

// MythUI headers
#include "mythimage.h"
#include "mythuiimage.h"
#include "mythuigroup.h"
#include "mythuitext.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUIStateType::MythUIStateType(MythUIType *parent, const QString &name)
    : MythUIComposite(parent, name)
{
    emit DependChanged(false);
}

bool MythUIStateType::AddImage(const QString &name, MythImage *image)
{
    QString key = name.toLower();

    if (m_ObjectsByName.contains(key) || !image)
        return false;

    // Uses name, not key which is lower case otherwise we break
    // inheritance
    auto *imType = new MythUIImage(this, name);
    imType->SetImage(image);

    return AddObject(key, imType);
}

bool MythUIStateType::AddObject(const QString &name, MythUIType *object)
{
    QString key = name.toLower();

    if (m_ObjectsByName.contains(key) || !object)
        return false;

    object->SetVisible(false);
    m_ObjectsByName[key] = object;

    MythRect objectArea = object->GetArea();
    objectArea.CalculateArea(m_ParentArea);

    ExpandArea(objectArea);

    return true;
}

bool MythUIStateType::AddImage(StateType type, MythImage *image)
{
    if (m_ObjectsByState.contains((int)type) || !image)
        return false;

    QString name = QString("stateimage%1").arg(type);

    auto *imType = new MythUIImage(this, name);
    imType->SetImage(image);

    return AddObject(type, imType);
}

bool MythUIStateType::AddObject(StateType type, MythUIType *object)
{
    if (m_ObjectsByState.contains((int)type) || !object)
        return false;

    object->SetVisible(false);
    m_ObjectsByState[(int)type] = object;

    MythRect objectArea = object->GetArea();
    objectArea.CalculateArea(m_ParentArea);

    ExpandArea(objectArea);

    return true;
}

bool MythUIStateType::DisplayState(const QString &name)
{
    if (name.isEmpty())
        return false;

    MythUIType *old = m_CurrentState;

    QMap<QString, MythUIType *>::Iterator i = m_ObjectsByName.find(name.toLower());

    if (i != m_ObjectsByName.end())
        m_CurrentState = i.value();
    else
        m_CurrentState = nullptr;

    if (m_CurrentState != old)
    {
        if (m_ShowEmpty || m_CurrentState)
        {
            if (m_deferload && m_CurrentState)
                m_CurrentState->LoadNow();

            if (old)
                old->SetVisible(false);

            if (m_CurrentState)
                m_CurrentState->SetVisible(true);
        }
    }
    AdjustDependence();

    return (m_CurrentState != nullptr);
}

bool MythUIStateType::DisplayState(StateType type)
{
    MythUIType *old = m_CurrentState;

    QMap<int, MythUIType *>::Iterator i = m_ObjectsByState.find((int)type);

    if (i != m_ObjectsByState.end())
        m_CurrentState = i.value();
    else
        m_CurrentState = nullptr;

    if (m_CurrentState != old)
    {
        if (m_ShowEmpty || m_CurrentState)
        {
            if (m_deferload && m_CurrentState)
                m_CurrentState->LoadNow();

            if (old)
                old->SetVisible(false);

            if (m_CurrentState)
                m_CurrentState->SetVisible(true);
        }
    }
    AdjustDependence();

    return (m_CurrentState != nullptr);
}

MythUIType *MythUIStateType::GetState(const QString &name)
{
    QString lcname = name.toLower();

    if (m_ObjectsByName.contains(lcname))
        return m_ObjectsByName[lcname];

    return nullptr;
}

MythUIType *MythUIStateType::GetState(StateType state)
{
    if (m_ObjectsByState.contains(state))
        return m_ObjectsByState[state];

    return nullptr;
}

/*!
 * \copydoc MythUIType::Clear()
 */
void MythUIStateType::Clear()
{
    if (m_ObjectsByName.isEmpty() && m_ObjectsByState.isEmpty())
        return;

    QMap<QString, MythUIType *>::Iterator i;

    for (i = m_ObjectsByName.begin(); i != m_ObjectsByName.end(); ++i)
    {
        DeleteChild(i.value());
    }

    QMap<int, MythUIType *>::Iterator j;

    for (j = m_ObjectsByState.begin(); j != m_ObjectsByState.end(); ++j)
    {
        DeleteChild(j.value());
    }

    m_ObjectsByName.clear();
    m_ObjectsByState.clear();

    m_CurrentState = nullptr;
    SetRedraw();
}

/*!
 * \copydoc MythUIType::Reset()
 */
void MythUIStateType::Reset()
{
    if (!DisplayState("default") && !DisplayState("active"))
    {
        if (!DisplayState(None))
        {
            if (m_CurrentState)
                m_CurrentState->SetVisible(false);

            m_CurrentState = nullptr;
        }
    }

    MythUIType::Reset();
}

bool MythUIStateType::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    QMap<QString, QString> dependsMap;
    if (element.tagName() == "showempty")
        m_ShowEmpty = parseBool(element);
    else if (element.tagName() == "state")
    {
        QString name = element.attribute("name", "");
        QString type = element.attribute("type", "");

        QString statename;

        if (!type.isEmpty())
            statename = type;
        else
            statename = name;

        element.setAttribute("name", statename);

        MythUIGroup *uitype = dynamic_cast<MythUIGroup *>
                              (ParseUIType(filename, element, "group", this, nullptr, showWarnings, dependsMap));

        if (!type.isEmpty())
        {
            StateType stype = None;

            if (type == "off")
                stype = Off;
            else if (type == "half")
                stype = Half;
            else if (type == "full")
                stype = Full;

            if (uitype && !m_ObjectsByState.contains((int)stype))
                AddObject(stype, uitype);
        }
        else if (!name.isEmpty())
        {
            if (uitype && !m_ObjectsByName.contains(name))
                AddObject(name, uitype);
        }
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIStateType::CopyFrom(MythUIType *base)
{
    auto *st = dynamic_cast<MythUIStateType *>(base);

    if (!st)
        return;

    m_ShowEmpty = st->m_ShowEmpty;

    MythUIType::CopyFrom(base);

    QMap<QString, MythUIType *>::iterator i;

    for (i = st->m_ObjectsByName.begin(); i != st->m_ObjectsByName.end(); ++i)
    {
        MythUIType *other = i.value();
        const QString& key = i.key();

        MythUIType *newtype = GetChild(other->objectName());
        AddObject(key, newtype);
        newtype->SetVisible(false);
    }

    QMap<int, MythUIType *>::iterator j;

    for (j = st->m_ObjectsByState.begin(); j != st->m_ObjectsByState.end(); ++j)
    {
        MythUIType *other = j.value();
        int key = j.key();

        MythUIType *newtype = GetChild(other->objectName());
        AddObject((StateType)key, newtype);
        newtype->SetVisible(false);
    }
}

void MythUIStateType::CreateCopy(MythUIType *parent)
{
    auto *st = new MythUIStateType(parent, objectName());
    st->CopyFrom(this);
}

void MythUIStateType::Finalize(void)
{
    if (!DisplayState("default"))
        DisplayState(None);
}

void MythUIStateType::EnsureStateLoaded(const QString &name)
{
    if (name.isEmpty())
        return;

    QMap<QString, MythUIType *>::Iterator i = m_ObjectsByName.find(name);

    if (i != m_ObjectsByName.end())
        i.value()->LoadNow();
}

void MythUIStateType::EnsureStateLoaded(StateType type)
{
    QMap<int, MythUIType *>::Iterator i = m_ObjectsByState.find((int)type);

    if (i != m_ObjectsByState.end())
        i.value()->LoadNow();
}

void MythUIStateType::LoadNow(void)
{
    if (!m_deferload)
        MythUIType::LoadNow();
}

void MythUIStateType::RecalculateArea(bool recurse)
{
    if (m_Parent)
    {
        if (objectName().startsWith("buttonlist button"))
        {
            auto *list = static_cast<MythUIButtonList *>(m_Parent);
            m_ParentArea = list->GetButtonArea();
        }
        else
            m_ParentArea = m_Parent->GetFullArea();
    }
    else
        m_ParentArea = GetMythMainWindow()->GetUIScreenRect();

    m_Area.Reset();
    m_Area.CalculateArea(m_ParentArea);

    if (recurse)
    {
        QList<MythUIType *>::iterator it;

        for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        {
            (*it)->RecalculateArea(recurse);
        }
    }
}

void MythUIStateType::AdjustDependence(void)
{
    if (m_CurrentState == nullptr || !m_CurrentState->IsVisible())
    {
        emit DependChanged(true);
        return;
    }
    QList<MythUIType *> *children = m_CurrentState->GetAllChildren();
    foreach (auto & child, *children)
    {
        if (child->IsVisible())
        {
            emit DependChanged(false);
            return;
        }
    }
    emit DependChanged(true);
}

void MythUIStateType::SetTextFromMap(const InfoMap &infoMap)
{
    if (m_ObjectsByName.isEmpty() && m_ObjectsByState.isEmpty())
        return;

    QMap<QString, MythUIType *>::Iterator i;

    for (i = m_ObjectsByName.begin(); i != m_ObjectsByName.end(); ++i)
    {
        MythUIType *type = i.value();

        auto *textType = dynamic_cast<MythUIText *> (type);
        if (textType)
            textType->SetTextFromMap(infoMap);

        auto *group = dynamic_cast<MythUIComposite *> (type);
        if (group)
            group->SetTextFromMap(infoMap);
    }

    QMap<int, MythUIType *>::Iterator j;

    for (j = m_ObjectsByState.begin(); j != m_ObjectsByState.end(); ++j)
    {
        MythUIType *type = j.value();

        auto *textType = dynamic_cast<MythUIText *> (type);
        if (textType)
            textType->SetTextFromMap(infoMap);

        auto *group = dynamic_cast<MythUIComposite *> (type);
        if (group)
            group->SetTextFromMap(infoMap);
    }
}
