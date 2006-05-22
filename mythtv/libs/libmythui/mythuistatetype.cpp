#include "mythuistatetype.h"
#include "mythuiimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

#include "mythcontext.h"

MythUIStateType::MythUIStateType(MythUIType *parent, const char *name)
                : MythUIType(parent, name)
{
    m_CurrentState = NULL;
    m_ShowEmpty = false;
}

MythUIStateType::~MythUIStateType()
{
}

bool MythUIStateType::AddImage(const QString &name, MythImage *image)
{
    if (m_ObjectsByName.contains(name) || !image)
        return false;

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
    if (m_ObjectsByState.contains((int)type) || !image)
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
    MythUIType *old = m_CurrentState;

    QMap<QString, MythUIType *>::Iterator i = m_ObjectsByName.find(name);
    if (i != m_ObjectsByName.end())
        m_CurrentState = i.data();
    else
        m_CurrentState = NULL;

    if (m_CurrentState != old)
    {
        if (m_ShowEmpty || m_CurrentState != NULL)
        {
            if (m_CurrentState)
                m_CurrentState->SetVisible(true);
            if (old)
                old->SetVisible(false);
        }
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
        if (m_ShowEmpty || m_CurrentState != NULL)
        {
            if (m_CurrentState)
                m_CurrentState->SetVisible(true);
            if (old)
                old->SetVisible(false);
        }
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

bool MythUIStateType::ParseElement(QDomElement &element)
{
    if (element.tagName() == "showempty")
        m_ShowEmpty = parseBool(element);
    else if (element.tagName() == "state")
    {
        QString name = element.attribute("name", "").lower();
        QString type = element.attribute("type", "").lower();

        MythUIType *uitype = ParseChildren(element, this);

        if (!type.isEmpty())
        {
            StateType stype = None;
            if (type == "off")
                stype = Off;
            else if (type == "half")
                stype = Half;
            else if (type == "full")
                stype = Full;

            if (uitype && m_ObjectsByState.contains((int)stype))
            {
                delete m_ObjectsByState[(int)stype]; 
                m_ObjectsByState.erase((int)stype);
            }
            AddObject(stype, uitype);
        }
        else if (!name.isEmpty())
        {
            if (uitype && m_ObjectsByName.contains(name))
            {
                delete m_ObjectsByName[name];
                m_ObjectsByName.erase(name);
            }
            AddObject(name, uitype);
        }
    }
    else
        return MythUIType::ParseElement(element);

    return true;
}
        
void MythUIStateType::CopyFrom(MythUIType *base)
{
    MythUIStateType *st = dynamic_cast<MythUIStateType *>(base);
    if (!st)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    ClearMaps();

    m_ShowEmpty = st->m_ShowEmpty;

    MythUIType::CopyFrom(base);

    QMap<QString, MythUIType *>::iterator i;
    for (i = st->m_ObjectsByName.begin(); i != st->m_ObjectsByName.end(); ++i)
    {
         MythUIType *other = i.data();
         QString key = i.key();

         MythUIType *newtype = GetChild(other->name());
         AddObject(key, newtype);
    }

    QMap<int, MythUIType *>::iterator j;
    for (j = st->m_ObjectsByState.begin(); j != st->m_ObjectsByState.end(); ++j)
    {
         MythUIType *other = j.data();
         int key = j.key();

         MythUIType *newtype = GetChild(other->name());
         AddObject((StateType)key, newtype);
    }
}

void MythUIStateType::CreateCopy(MythUIType *parent)
{
    MythUIStateType *st = new MythUIStateType(parent, name());
    st->CopyFrom(this);
}

void MythUIStateType::Finalize(void)
{
}

