#ifndef LIBVISUALPLUGIN_H
#define LIBVISUALPLUGIN_H

#include "mainvisual.h"
#include "config.h"

#if defined(SDL_SUPPORT) && defined(LIBVISUAL_SUPPORT) 
extern "C"
{
    #include <libvisual/libvisual.h>
    #include <SDL.h>
}

class LibVisualPlugin : public VisualBase
{
  public:
    LibVisualPlugin(MainVisual *parent,
                    long int winid, const QString &pluginName);
    virtual ~LibVisualPlugin();

    void resize(const QSize &size);
    bool process(VisualNode *node = 0);
    bool draw(QPainter *p, const QColor &back = Qt::black);
    void handleKeyPress(const QString &action);

    static uint plugins(QStringList *list);

  private:
    static int AudioCallback(VisInput *input, VisAudio *audio, void *priv);
    void switchToPlugin(const QString &pluginName);
    bool createScreen(int width, int height);

  private:
    MainVisual  *m_parent;
    QStringList  m_pluginList;       ///< list of available libvisual plugins
    uint         m_currentPlugin;
    VisBin      *m_pVisBin;          ///< Pointer to LibVisual representation
                                     ///<  of an application
    VisVideo    *m_pVisVideo;        ///< Pointer to LibVisual representation
                                     ///<  of a screen surface

    SDL_Surface *m_pSurface;         ///< Pointer to SDL representation
                                     ///<  of rendering surface

    int16_t      m_Audio[2][512];    ///< PCM audio data transfer buffer
    bool         m_paused;
};


#endif // SDL_SUPPORT && LIBVISUAL_SUPPORT

#endif // LIBVISUALPLUGIN_H
