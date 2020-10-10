#ifndef MYTHPLAYERVISUALISER_H
#define MYTHPLAYERVISUALISER_H

// MythTV
#include "visualisations/videovisual.h"
#include "mythplayeruibase.h"

class MythPlayerVisualiser : public MythPlayerUIBase
{
    Q_OBJECT

  public slots:
    void        EmbedVisualiser(bool Embed, const QRect& Rect = {});

  public:
    explicit MythPlayerVisualiser(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerVisualiser();

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
