#include "mythgoom.h"

// C++
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

// Qt
#include <QCoreApplication>
#include <QPainter>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/compat.h>
#include <libmythbase/mythlogging.h>

// Goom
#include "libmythtv/visualisations/goom/goom_tools.h"
#include "libmythtv/visualisations/goom/goom_core.h"

Goom::Goom()
{
    m_fps = 20;

    goom_init(800, 600, 0);

    m_scalew = gCoreContext->GetNumSetting("VisualScaleWidth", 2);
    m_scaleh = gCoreContext->GetNumSetting("VisualScaleHeight", 2);

    // we allow 1, 2 or 4 for the scale since goom likes its resolution to be a multiple of 2
    if (m_scaleh == 3 || m_scaleh > 4)
        m_scaleh = 4;
    m_scaleh = std::max(m_scaleh, 1);

    if (m_scalew == 3 || m_scalew > 4)
        m_scalew = 4;
    m_scalew = std::max(m_scalew, 1);
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
    if (!node || node->m_length == 0)
        return false;

    int numSamps = 512;
    if (node->m_length < 512)
        numSamps = node->m_length;

    GoomDualData data;

    int i = 0;
    for (i = 0; i < numSamps; i++)
    {
        data[0][i] = node->m_left[i];
        if (node->m_right)
            data[1][i] = node->m_right[i];
        else
            data[1][i] = data[0][i];
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

    auto *image = new QImage((uchar*) m_buffer, width, height, width * 4, QImage::Format_RGB32);

    p->drawImage(QRect(0, 0, m_size.width(), m_size.height()), *image);

    delete image;

    return true;
}

static class GoomFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Goom");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create([[maybe_unused]] MainVisual *parent,
                       [[maybe_unused]] const QString &pluginName) const override // VisFactory
    {
        return new Goom();
    }
}GoomFactory;
