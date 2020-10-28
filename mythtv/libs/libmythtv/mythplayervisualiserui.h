#ifndef MYTHPLAYERVISUALISERUI_H
#define MYTHPLAYERVISUALISERUI_H

// MythTV
#include "visualisations/videovisual.h"
#include "mythplayervideoui.h"

class MTV_PUBLIC MythPlayerVisualiserUI : public MythPlayerVideoUI
{
    Q_OBJECT

  signals:
    void        VisualiserStateChanged(MythVisualiserState VisualiserState);

  protected slots:
    void        UIScreenRectChanged(const QRect& Rect);
    void        EmbedVisualiser(bool Embed, const QRect& Rect = {});
    void        EnableVisualiser(bool Enable, bool Toggle, const QString& Name);
    void        AudioPlayerStateChanged(MythAudioPlayerState State);

  public:
    MythPlayerVisualiserUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerVisualiserUI() override;

  protected:
    void        PrepareVisualiser();
    void        RenderVisualiser();
    void        AutoVisualise(bool HaveVideo);

  private:
    void        DestroyVisualiser();

    QString      m_defaultVisualiser { };
    MythVisualiserState m_visualiserState { };
    VideoVisual* m_visual      { nullptr };
    QRect        m_uiScreenRect{ };
    QRect        m_embedRect   { };
};

#endif
