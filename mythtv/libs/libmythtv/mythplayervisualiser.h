#ifndef MYTHPLAYERVISUALISER_H
#define MYTHPLAYERVISUALISER_H

// MythTV
#include "visualisations/videovisual.h"

class MythMainWindow;

class MythPlayerVisualiser
{
  public:
    explicit MythPlayerVisualiser(AudioPlayer* Audio);
   ~MythPlayerVisualiser();

    void        PrepareVisualiser();
    void        RenderVisualiser();
    bool        CanVisualise();
    bool        IsVisualising();
    QString     GetVisualiserName();
    QStringList GetVisualiserList();
    bool        EnableVisualiser(bool Enable, const QString& Name = QString(""));
    void        AutoVisualise(bool HaveVideo);
    void        EmbedVisualiser(bool Embed, const QRect& Rect = {});
    bool        VisualiserIsEmbedding();
    QRect       GetEmbeddingRect();

  private:
    void        DestroyVisualiser();

    MythMainWindow* m_mainWindow { nullptr };
    MythRender*     m_render     { nullptr };
    MythPainter*    m_painter    { nullptr };
    AudioPlayer*    m_audio      { nullptr };
    VideoVisual*    m_visual     { nullptr };
    QRect           m_embedRect  { 0, 0, 0, 0};
    bool            m_embedding  { false   };
};

#endif
