#include "mythuistatetype.h"
#include "mythuiimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythcontext.h"

#define LOC QString("MythUIStateType: ")
#define LOC_ERR QString("MythUIStateType, Error: ")

MythUIStateType::MythUIStateType(MythUIType *parent, const char *name)
                : MythUIType(parent, name)
{
    m_CurrentState = NULL;
}

MythUIStateType::~MythUIStateType()
{
}

bool MythUIStateType::AddImage(const QString &name, MythImage *image)
{
    if (m_ObjectsByName.contains(name))
        return false;

    if (!image)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("AddImage() failed to add '%1'").arg(name));

        return false;
    }

    MythUIImage *imType = new MythUIImage(this, name);
    imType->SetImage(image);

    return AddObject(name, imType);
}

bool MythUIStateType::AddObject(const QString &name, MythUIType *object)
{
    if (m_ObjectsByName.contains(name) || !object)
        return false;

    object->SetVisible(false);
    m_ObjectsByName[name] = object;

    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(object->GetArea().size());
    m_Area.setSize(aSize);

    return true;
}

bool MythUIStateType::AddImage(StateType type, MythImage *image)
{
    if (m_ObjectsByState.contains((int)type))
        return false;

    MythUIImage *imType = new MythUIImage(this, "stateimage");
    imType->SetImage(image);

    return AddObject(type, imType);
}

bool MythUIStateType::AddObject(StateType type, MythUIType *object)
{
    if (m_ObjectsByState.contains((int)type) || !object)
        return false;

    object->SetVisible(false);
    m_ObjectsByState[(int)type] = object;

    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(object->GetArea().size());
    m_Area.setSize(aSize);

    return true;
}

bool MythUIStateType::DisplayState(const QString &name)
{
    if (name.isEmpty())
        return false;

    MythUIType *old = m_CurrentState;

    QMap<QString, MythUIType *>::Iterator i = m_ObjectsByName.find(name);
    if (i != m_ObjectsByName.end())
        m_CurrentState = i.data();
    else
        m_CurrentState = NULL;

    if (m_CurrentState != old)
    {
        if (m_CurrentState)
            m_CurrentState->SetVisible(true);
        if (old)
            old->SetVisible(false);
    }

    return (m_CurrentState != NULL);
}

bool MythUIStateType::DisplayState(StateType type)
{
    MythUIType *old = m_CurrentState;

    QMap<int, MythUIType *>::Iterator i = m_ObjectsByState.find((int)type);
    if (i != m_ObjectsByState.end())
        m_CurrentState = i.data();
    else
        m_CurrentState = NULL;

    if (m_CurrentState != old)
    {
        if (m_CurrentState)
            m_CurrentState->SetVisible(true);
        if (old)
            old->SetVisible(false);
    }

    return (m_CurrentState != NULL);
}

void MythUIStateType::ClearMaps()
{
    QMap<QString, MythUIType *>::Iterator i;
    for (i = m_ObjectsByName.begin(); i != m_ObjectsByName.end(); ++i)
    {
        delete i.data();
    }

    QMap<int, MythUIType *>::Iterator j;
    for (j = m_ObjectsByState.begin(); j != m_ObjectsByState.end(); ++j)
    {
        delete j.data();
    }

    m_ObjectsByName.clear();
    m_ObjectsByState.clear();

    m_CurrentState = NULL;
}

void MythUIStateType::ClearImages()
{
    ClearMaps();
    SetRedraw();
}

