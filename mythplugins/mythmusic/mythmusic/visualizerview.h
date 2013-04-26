#ifndef VISUALIZERVIEW_H_
#define VISUALIZERVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// mythui
#include <mythscreentype.h>

// mythmusic
#include <musiccommon.h>

class MythUIVideo;

class VisualizerView : public MusicCommon
{
    Q_OBJECT
  public:
    VisualizerView(MythScreenStack *parent);
    ~VisualizerView(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    virtual void ShowMenu(void);

  protected:
    void customEvent(QEvent *event);

  private slots:
    void showTrackInfoPopup(void);
};

class MPUBLIC TrackInfoPopup : public MythScreenType
{
  Q_OBJECT
  public:
    TrackInfoPopup(MythScreenStack *parent, MusicMetadata *mdata);
    ~TrackInfoPopup(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

  protected:
    MusicMetadata *m_metadata;
    QTimer   *m_displayTimer;
};

#endif
