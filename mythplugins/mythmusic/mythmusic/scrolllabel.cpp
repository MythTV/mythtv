// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "scrolllabel.h"

#include <qpainter.h>
#include <qtimer.h>


ScrollLabel::ScrollLabel(QWidget *parent, const char *name)
    : QWidget(parent, name), offx(0), offend(0), atend(0), speed(0), delay(0)
{
    timer = new QTimer;
    connect(timer, SIGNAL(timeout()), SLOT(slotTimeout()));

    setScrollSpeed(70);
}

void ScrollLabel::paletteChange( const QPalette & )
{
    setText( txt );
}

void ScrollLabel::fontChange( const QFont & )
{
    setText( txt );
}

QSize ScrollLabel::sizeHint() const
{
    QFontMetrics fm(font());
    return QSize(128, fm.height());
}


QSize ScrollLabel::minimumSizeHint() const
{
    return sizeHint();
}


void ScrollLabel::setScrollSpeed(int ss)
{
    speed = 100 - ss;
    setStopDelay(delay);
    timer->stop();
    timer->start(speed, FALSE);
}


void ScrollLabel::setStopDelay(int sd)
{
    delay = sd;
}


void ScrollLabel::setText(const QString &newtext)
{
    txt = newtext;

    // update pixmap
    QFontMetrics fm(font());
    QSize sz(fm.width(txt), fm.height());
    sz = sz.expandedTo(size());
    pm.resize(sz);

    if (offx + width() >= pm.width()) {
	offend = 1;
	offx = pm.width() - width();
    }

    if (offx <= 0) {
	offend = 0;
	offx = 0;
    }

    pm.fill(backgroundColor());

    {
	QPainter p(&pm, this);
	p.setPen(foregroundColor());
	p.drawText(0, 0, pm.width(), pm.height(), AlignCenter, txt);
    }

    bitBlt(this, 0, 0, &pm, offx, 0);
    erase(offx + pm.width(), 0, -1, -1);
}


void ScrollLabel::slotTimeout()
{
    if (atend-- > 0)
	return;

    if (offend)
	offx--;
    else
	offx++;

    if (offx + width() >= pm.width()) {
	atend = delay;
	offend = 1;
	offx = pm.width() - width();
    }

    if (offx <= 0) {
	atend = delay;
	offend = 0;
	offx = 0;
    }

    bitBlt(this, 0, 0, &pm, offx, 0);
}


void ScrollLabel::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setClipRegion(event->region());
    p.drawPixmap(0, 0, pm, offx, 0);
}


void ScrollLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    setText(text());
}

