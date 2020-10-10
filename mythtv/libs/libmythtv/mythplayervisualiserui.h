#ifndef MYTHPLAYERVISUALISERUI_H
#define MYTHPLAYERVISUALISERUI_H

// MythTV
#include "visualisations/videovisual.h"
#include "mythplayeruibase.h"

class MythPlayerVisualiserUI : public MythPlayerUIBase
{
    Q_OBJECT

  public slots:
    void        EmbedVisualiser(bool Embed, const QRect& Rect = {});

  public:
    explicit MythPlayerVisualiserUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerVisualiserUI();

    void        PrepareVisualiser();
    void        RenderVisualiser();
    bool        CanVisualise();
    bool        IsVisualising();
    QString     GetVisualiserName();
    QStringList GetVisualiserList();
    bool        EnableVisualiser(bool Enable, const QString& Name = QString(""));
    void        AutoVisualise(bool HaveVideo);
    bool        IsEmbedding();

  private:
    void        DestroyVisualiser();

    VideoVisual* m_visual    { nullptr };
    QRect        m_embedRect { 0, 0, 0, 0};
    bool         m_embedding { false   };
};

#endif
