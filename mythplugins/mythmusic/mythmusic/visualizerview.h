#ifndef VISUALIZERVIEW_H_
#define VISUALIZERVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// MythTV
#include <libmythbase/mythpluginexport.h>
#include <libmythui/mythscreentype.h>

// mythmusic
#include "musiccommon.h"

class MythUIVideo;

class VisualizerView : public MusicCommon
{
    Q_OBJECT
  public:
    VisualizerView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~VisualizerView(void) override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MusicCommon

    void ShowMenu(void) override; // MusicCommon

  protected:
    void customEvent(QEvent *event) override; // MusicCommon

  private slots:
    static void showTrackInfoPopup(void);
};

class MPLUGIN_PUBLIC TrackInfoPopup : public MythScreenType
{
  Q_OBJECT
  public:
    TrackInfoPopup(MythScreenStack *parent, MusicMetadata *mdata)
        : MythScreenType(parent, "trackinfopopup", false),
        m_metadata(mdata) {}
    ~TrackInfoPopup(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  protected:
    MusicMetadata *m_metadata     {nullptr};
    QTimer        *m_displayTimer {nullptr};
};

#endif
