// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __mainvisual_h
#define __mainvisual_h

#include <vector>
using namespace std;

#include "constants.h"

#include <QResizeEvent>
#include <QPaintEvent>
#include <QStringList>
#include <QHideEvent>
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QList>

// MythTV headers
#include <visual.h>

// MythMusic
#include "visualize.h"

class MythUIVideo;

// base class to handle things like frame rate...
class MainVisual :  public QObject, public MythTV::Visual
{
    Q_OBJECT

  public:
    MainVisual(MythUIVideo *visualizer);
    virtual ~MainVisual();

    VisualBase *visual(void) const { return m_vis; }
    void setVisual(const QString &name);

    void stop(void);

    void resize(const QSize &size);

    void add(uchar *, unsigned long, unsigned long, int, int);
    void prepare(void);

    void customEvent(QEvent *);

    void setFrameRate(int newfps);
    int frameRate(void) const { return m_fps; }

    QStringList getVisualizations(void) { return m_visualizers; }

    int getCurrentVisual(void) { return m_currentVisualizer; }

  public slots:
    void timeout();

  private:
    MythUIVideo *m_visualizerVideo;
    QStringList m_visualizers;
    int m_currentVisualizer;
    VisualBase *m_vis;
    QPixmap m_pixmap;
    QList<VisualNode*> m_nodes;
    bool m_playing;
    int m_fps;
    unsigned long m_samples;
    QTimer *m_updateTimer;
};

#endif // __mainvisual_h

