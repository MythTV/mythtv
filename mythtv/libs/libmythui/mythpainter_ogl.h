#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

#include <QMutex>

#include <list>

#include "mythpainter.h"
#include "mythimage.h"

class QWidget;
class MythRenderOpenGL;

class MUI_PUBLIC MythOpenGLPainter : public MythPainter
{
    friend class VideoOutputOpenGL;
  public:
    MythOpenGLPainter(MythRenderOpenGL *render =  nullptr, QWidget *parent = nullptr);
   ~MythOpenGLPainter();

    void SetTarget(int new_target)       { target = new_target;      }
    void SetSwapControl(bool swap)       { swapControl = swap;       } 
    QString GetName(void) override // MythPainter
        { return QString("OpenGL"); }
    bool SupportsAnimation(void) override // MythPainter
        { return true; }
    bool SupportsAlpha(void) override // MythPainter
        { return true; }
    bool SupportsClipping(void) override // MythPainter
        { return false; }
    void FreeResources(void) override; // MythPainter
    void Begin(QPaintDevice *parent) override; // MythPainter
    void End() override; // MythPainter

    void DrawImage(const QRect &r, MythImage *im, const QRect &src,
                   int alpha) override; // MythPainter
    void DrawRect(const QRect &area, const QBrush &fillBrush,
                  const QPen &linePen, int alpha) override; // MythPainter
    void DrawRoundRect(const QRect &area, int cornerRadius,
                       const QBrush &fillBrush, const QPen &linePen,
                       int alpha) override; // MythPainter

    void PushTransformation(const UIEffects &fx, QPointF center = QPointF()) override; // MythPainter
    void PopTransformation(void) override; // MythPainter
    void       DeleteTextures(void);

  protected:
    MythImage* GetFormatImagePriv(void) override // MythPainter
        { return new MythImage(this); }
    void DeleteFormatImagePriv(MythImage *im) override; // MythPainter

    void       ClearCache(void);
    int        GetTextureFromCache(MythImage *im);

    QWidget          *realParent  {nullptr};
    MythRenderOpenGL *realRender  {nullptr};
    int               target      {0};
    bool              swapControl {true};

    QMap<MythImage *, uint>    m_ImageIntMap;
    std::list<MythImage *>     m_ImageExpireList;
    std::list<uint>            m_textureDeleteList;
    QMutex                     m_textureDeleteLock;
};

#endif
