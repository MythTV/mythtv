#include "mythuistateimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"

MythUIStateImage::MythUIStateImage(MythUIType *parent, const char *name)
                : MythUIType(parent, name)
{
    m_CurrentImage = NULL;
}

MythUIStateImage::~MythUIStateImage()
{
    ClearMaps();
}

bool MythUIStateImage::AddImage(const QString &name, MythImage *image)
{
    if (m_ImagesByName.contains(name))
        return false;

    m_ImagesByName[name] = image;
    image->UpRef();

    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(image->size());
    m_Area.setSize(aSize);

    return true;
}

bool MythUIStateImage::AddImage(StateType type, MythImage *image)
{
    if (m_ImagesByState.contains((int)type))
        return false;

    m_ImagesByState[(int)type] = image;
    image->UpRef();

    QSize aSize = m_Area.size();
    aSize = aSize.expandedTo(image->size());
    m_Area.setSize(aSize);

    return true;
}

bool MythUIStateImage::DisplayImage(const QString &name)
{
    MythImage *old = m_CurrentImage;

    QMap<QString, MythImage *>::Iterator i = m_ImagesByName.find(name);
    if (i != m_ImagesByName.end())
        m_CurrentImage = i.data();
    else
        m_CurrentImage = NULL;

    if (m_CurrentImage != old)
        SetRedraw();

    return (m_CurrentImage != NULL);
}

bool MythUIStateImage::DisplayImage(StateType type)
{
    MythImage *old = m_CurrentImage;

    QMap<int, MythImage *>::Iterator i = m_ImagesByState.find((int)type);
    if (i != m_ImagesByState.end())
        m_CurrentImage = i.data();
    else
        m_CurrentImage = NULL;

    if (m_CurrentImage != old)
        SetRedraw();

    return (m_CurrentImage != NULL);
}

void MythUIStateImage::ClearMaps()
{
    QMap<QString, MythImage *>::Iterator i;
    for (i = m_ImagesByName.begin(); i != m_ImagesByName.end(); ++i)
    {
        i.data()->DownRef();;
    }

    QMap<int, MythImage *>::Iterator j;
    for (j = m_ImagesByState.begin(); j != m_ImagesByState.end(); ++j)
    {
        j.data()->DownRef();
    }

    m_ImagesByName.clear();
    m_ImagesByState.clear();

    m_CurrentImage = NULL;
}

void MythUIStateImage::ClearImages()
{
    ClearMaps();
    SetRedraw();
}

void MythUIStateImage::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                                int alphaMod, QRect clipRect)
{
    if (m_CurrentImage)
    {
        QRect area = m_Area;
        area.moveBy(xoffset, yoffset);

        int alpha = CalcAlpha(alphaMod); 

        QRect srcRect = m_CurrentImage->rect();
        p->DrawImage(area, m_CurrentImage, srcRect, alpha);
    }
}

