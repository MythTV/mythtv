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

    if (m_objectsByName.contains(key) || !image)
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

    if (m_objectsByName.contains(key) || !object)
        return false;

    object->SetVisible(false);
    m_objectsByName[key] = object;

    MythRect objectArea = object->GetArea();
    objectArea.CalculateArea(m_parentArea);

    ExpandArea(objectArea);

    return true;
}

bool MythUIStateType::AddImage(StateType type, MythImage *image)
{
    if (m_objectsByState.contains((int)type) || !image)
        return false;

    QString name = QString("stateimage%1").arg(type);

    auto *imType = new MythUIImage(this, name);
    imType->SetImage(image);

    return AddObject(type, imType);
}

bool MythUIStateType::AddObject(StateType type, MythUIType *object)
{
    if (m_objectsByState.contains((int)type) || !object)
        return false;

    object->SetVisible(false);
    m_objectsByState[(int)type] = object;

    MythRect objectArea = object->GetArea();
    objectArea.CalculateArea(m_parentArea);

    ExpandArea(objectArea);

    return true;
}

bool MythUIStateType::DisplayState(const QString &name)
{
    if (name.isEmpty())
        return false;

    MythUIType *old = m_currentState;

    QMap<QString, MythUIType *>::Iterator i = m_objectsByName.find(name.toLower());

    if (i != m_objectsByName.end())
        m_currentState = i.value();
    else
        m_currentState = nullptr;

    if (m_currentState != old)
    {
        if (m_showEmpty || m_currentState)
        {
            if (m_deferload && m_currentState)
                m_currentState->LoadNow();

            if (old)
                old->SetVisible(false);

            if (m_currentState)
                m_currentState->SetVisible(true);
        }
    }
    AdjustDependence();

    return (m_currentState != nullptr);
}

bool MythUIStateType::DisplayState(StateType type)
{
    MythUIType *old = m_currentState;

    QMap<int, MythUIType *>::Iterator i = m_objectsByState.find((int)type);

    if (i != m_objectsByState.end())
        m_currentState = i.value();
    else
        m_currentState = nullptr;

    if (m_currentState != old)
    {
        if (m_showEmpty || m_currentState)
        {
            if (m_deferload && m_currentState)
                m_currentState->LoadNow();

            if (old)
                old->SetVisible(false);

            if (m_currentState)
                m_currentState->SetVisible(true);
        }
    }
    AdjustDependence();

    return (m_currentState != nullptr);
}

MythUIType *MythUIStateType::GetState(const QString &name)
{
    QString lcname = name.toLower();

    if (m_objectsByName.contains(lcname))
        return m_objectsByName[lcname];

    return nullptr;
}

MythUIType *MythUIStateType::GetState(StateType state)
{
    if (m_objectsByState.contains(state))
        return m_objectsByState[state];

    return nullptr;
}

/*!
 * \copydoc MythUIType::Clear()
 */
void MythUIStateType::Clear()
{
    if (m_objectsByName.isEmpty() && m_objectsByState.isEmpty())
        return;

    QMap<QString, MythUIType *>::Iterator i;

    for (i = m_objectsByName.begin(); i != m_objectsByName.end(); ++i)
    {
        DeleteChild(i.value());
    }

    QMap<int, MythUIType *>::Iterator j;

    for (j = m_objectsByState.begin(); j != m_objectsByState.end(); ++j)
    {
        DeleteChild(j.value());
    }

    m_objectsByName.clear();
    m_objectsByState.clear();

    m_currentState = nullptr;
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
            if (m_currentState)
                m_currentState->SetVisible(false);

            m_currentState = nullptr;
        }
    }

    MythUIType::Reset();
}

bool MythUIStateType::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    QMap<QString, QString> dependsMap;
    if (element.tagName() == "showempty")
        m_showEmpty = parseBool(element);
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

            if (uitype && !m_objectsByState.contains((int)stype))
                AddObject(stype, uitype);
        }
        else if (!name.isEmpty())
        {
            if (uitype && !m_objectsByName.contains(name))
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

    m_showEmpty = st->m_showEmpty;

    MythUIType::CopyFrom(base);

    QMap<QString, MythUIType *>::iterator i;

    for (i = st->m_objectsByName.begin(); i != st->m_objectsByName.end(); ++i)
    {
        MythUIType *other = i.value();
        const QString& key = i.key();

        MythUIType *newtype = GetChild(other->objectName());
        AddObject(key, newtype);
        newtype->SetVisible(false);
    }

    QMap<int, MythUIType *>::iterator j;

    for (j = st->m_objectsByState.begin(); j != st->m_objectsByState.end(); ++j)
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

    QMap<QString, MythUIType *>::Iterator i = m_objectsByName.find(name);

    if (i != m_objectsByName.end())
        i.value()->LoadNow();
}

void MythUIStateType::EnsureStateLoaded(StateType type)
{
    QMap<int, MythUIType *>::Iterator i = m_objectsByState.find((int)type);

    if (i != m_objectsByState.end())
        i.value()->LoadNow();
}

void MythUIStateType::LoadNow(void)
{
    if (!m_deferload)
        MythUIType::LoadNow();
}

void MythUIStateType::RecalculateArea(bool recurse)
{
    if (m_parent)
    {
        if (objectName().startsWith("buttonlist button"))
        {
            if (auto * list = dynamic_cast<MythUIButtonList *>(m_parent); list)
                m_parentArea = list->GetButtonArea();
        }
        else
        {
            m_parentArea = m_parent->GetFullArea();
        }
    }
    else
    {
        m_parentArea = GetMythMainWindow()->GetUIScreenRect();
    }

    m_area.Reset();
    m_area.CalculateArea(m_parentArea);

    if (recurse)
    {
        for (auto * child : std::as_const(m_childrenList))
            child->RecalculateArea(recurse);
    }
}

void MythUIStateType::AdjustDependence(void)
{
    if (m_currentState == nullptr || !m_currentState->IsVisible())
    {
        emit DependChanged(true);
        return;
    }
    QList<MythUIType *> *children = m_currentState->GetAllChildren();
    for (auto *child : std::as_const(*children))
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
    if (m_objectsByName.isEmpty() && m_objectsByState.isEmpty())
        return;

    QMap<QString, MythUIType *>::Iterator i;

    for (i = m_objectsByName.begin(); i != m_objectsByName.end(); ++i)
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

    for (j = m_objectsByState.begin(); j != m_objectsByState.end(); ++j)
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
