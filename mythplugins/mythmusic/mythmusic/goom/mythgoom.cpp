#include "mythgoom.h"

#include <cmath>
#include <cstdlib>

#include <iostream>
using namespace std;

#include <QCoreApplication>
#include <QPainter>

#include <compat.h>
#include <mythcontext.h>
#include <mythlogging.h>

extern "C" {
#include "goom_tools.h"
#include "goom_core.h"
}

Goom::Goom()
{
    m_fps = 20;

    m_buffer = NULL;

    goom_init(800, 600, 0);

    m_scalew = gCoreContext->GetNumSetting("VisualScaleWidth", 2);
    m_scaleh = gCoreContext->GetNumSetting("VisualScaleHeight", 2);

    // we allow 1, 2 or 4 for the scale since goom likes its resolution to be a multiple of 2
    if (m_scaleh == 3 || m_scaleh > 4)
        m_scaleh = 4;
    if (m_scaleh < 1)
        m_scaleh = 1;

    if (m_scalew == 3 || m_scalew > 4)
        m_scalew = 4;
    if (m_scalew < 1)
        m_scalew = 1;
}

Goom::~Goom()
{
    goom_close();
}

void Goom::resize(const QSize &newsize)
{
    m_size = newsize;

    m_size.setHeight((m_size.height() / 2) * 2);
    m_size.setWidth((m_size.width() / 2) * 2);

    // only scale the resolution if it is > 256
    // this ensures the small visualisers don't look too blocky
    if (m_size.width() > 256)
        goom_set_resolution(m_size.width() / m_scalew, m_size.height() / m_scaleh, 0);
    else
        goom_set_resolution(m_size.width(), m_size.height(), 0);
}

bool Goom::process(VisualNode *node)
{
    if (!node || node->length == 0)
        return false;

    int numSamps = 512;
    if (node->length < 512)
        numSamps = node->length;

    signed short int data[2][512];

    int i = 0;
    for (i = 0; i < numSamps; i++)
    {
        data[0][i] = node->left[i];
        if (node->right)
            data[1][i] = node->right[i];
        else
            data[1][i] = data[0][i];
    }

    for (; i < 512; i++)
    {
        data[0][i] = 0;
        data[1][i] = 0;
    }

    m_buffer = goom_update(data, 0);

    return false;
}

bool Goom::draw(QPainter *p, const QColor &back)
{
    p->fillRect(0, 0, m_size.width(), m_size.height(), back);

    if (!m_buffer)
        return true;

    int width = m_size.width();
    int height = m_size.height();

    if (m_size.width() > 256)
    {
        width /= m_scalew;
        height /= m_scaleh;
    }

    QImage *image = new QImage((uchar*) m_buffer, width, height, width * 4, QImage::Format_RGB32);

    p->drawImage(QRect(0, 0, m_size.width(), m_size.height()), *image);

    delete image;

    return true;
}

static class GoomFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name = QCoreApplication::translate("Visualizers",
                                                          "Goom");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new Goom();
    }
}GoomFactory;
