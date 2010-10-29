#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

#include <QMap>
#include <QMutex>
#include <QGLWidget>

#include <list>

#include "mythpainter.h"
#include "mythimage.h"
#include "mythrender_opengl.h"

class MPUBLIC MythOpenGLPainter : public MythPainter
{
  public:
    MythOpenGLPainter(MythRenderOpenGL *render =  NULL, QGLWidget *parent = NULL);
   ~MythOpenGLPainter();

    void SetTarget(int new_target)       { target = new_target;      }
    void SetSwapControl(bool swap)       { swapControl = swap;       } 
    virtual QString GetName(void)        { return QString("OpenGL"); }
    virtual bool SupportsAnimation(void) { return true;              }
    virtual bool SupportsAlpha(void)     { return true;              }
    virtual bool SupportsClipping(void)  { return false;             }
    virtual void FreeResources(void);
    virtual void Begin(QPaintDevice *parent);
    virtual void End();

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);
    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          const QRect &boundRect);
    virtual void DrawRect(const QRect &area,
                          bool drawFill, const QColor &fillColor, 
                          bool drawLine, int lineWidth, const QColor &lineColor);
    virtual void DrawRoundRect(const QRect &area, int radius, 
                               bool drawFill, const QColor &fillColor, 
                               bool drawLine, int lineWidth, const QColor &lineColor);

  protected:
    virtual void DeleteFormatImagePriv(MythImage *im);
    void       ExpireImages(uint max = 0);
    void       ClearCache(void);
    void       DeleteTextures(void);
    int        GetTextureFromCache(MythImage *im);
    MythImage *GetImageFromString(const QString &msg, int flags, const QRect &r,
                                  const MythFontProperties &font);
    MythImage *GetImageFromRect(const QSize &size, int radius,
                                bool drawFill, const QColor &fillColor,
                                bool drawLine, int lineWidth,
                                const QColor &lineColor);


    QGLWidget        *realParent;
    MythRenderOpenGL *realRender;
    int               target;
    bool              swapControl;

    QMap<MythImage *, uint>    m_ImageIntMap;
    std::list<MythImage *>     m_ImageExpireList;
    QMap<QString, MythImage *> m_StringToImageMap;
    std::list<QString>         m_StringExpireList;
    std::list<uint>            m_textureDeleteList;
    QMutex                     m_textureDeleteLock;
};

#endif
