// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __scrolllabel_h
#define __scrolllabel_h

#include <qwidget.h>
#include <qpixmap.h>

class QTimer;


class ScrollLabel : public QWidget
{
    Q_OBJECT
public:
    ScrollLabel(QWidget *parent, const char *name = 0);

    void paletteChange( const QPalette & );
    void fontChange( const QFont & );

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    void setText(const QString &newtext);
    const QString &text() const { return txt; }

    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);

    void setScrollSpeed(int);
    void setStopDelay(int);


private slots:
    void slotTimeout();


private:
    QTimer *timer;
    QString txt;
    QPixmap pm;
    int offx, offend, atend, speed, delay;
};


#endif // __scrolllabel_h
