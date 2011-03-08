#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

#include <QMutex>
#include <QGLWidget>

#include <list>

#include "mythpainter.h"
#include "mythimage.h"
#include "mythrender_opengl.h"

class MUI_PUBLIC MythOpenGLPainter : public MythPainter
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
    virtual void DrawRect(const QRect &area, const QBrush &fillBrush,
                          const QPen &linePen, int alpha);

  protected:
    virtual MythImage* GetFormatImagePriv(void) { return new MythImage(this); }
    virtual void DeleteFormatImagePriv(MythImage *im);

    void       ClearCache(void);
    void       DeleteTextures(void);
    int        GetTextureFromCache(MythImage *im);

    QGLWidget        *realParent;
    MythRenderOpenGL *realRender;
    int               target;
    bool              swapControl;

    QMap<MythImage *, uint>    m_ImageIntMap;
    std::list<MythImage *>     m_ImageExpireList;
    std::list<uint>            m_textureDeleteList;
    QMutex                     m_textureDeleteLock;
};

#endif
