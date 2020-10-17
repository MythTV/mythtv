#ifndef MYTHPLAYERVISUALISERUI_H
#define MYTHPLAYERVISUALISERUI_H

// MythTV
#include "visualisations/videovisual.h"
#include "mythplayervideoui.h"

class MTV_PUBLIC MythPlayerVisualiserUI : public MythPlayerVideoUI
{
    Q_OBJECT

  public slots:
    void        UIScreenRectChanged(const QRect& Rect);
    void        EmbedVisualiser(bool Embed, const QRect& Rect = {});

  public:
    MythPlayerVisualiserUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerVisualiserUI() override;

    void        PrepareVisualiser();
    void        RenderVisualiser();
    bool        CanVisualise();
    bool        IsVisualising();
    QString     GetVisualiserName();
    QStringList GetVisualiserList();
    bool        EnableVisualiser(bool Enable, const QString& Name = QString(""));
    void        AutoVisualise(bool HaveVideo);
    bool        IsEmbedding() const;

  private:
    void        DestroyVisualiser();

    VideoVisual* m_visual      { nullptr };
    QRect        m_uiScreenRect{ };
    QRect        m_embedRect   { };
    bool         m_embedding   { false   };
};

#endif
