#ifndef MYTHPAINTER_OPENGL_H_
#define MYTHPAINTER_OPENGL_H_

#include <QMap>
#include <QMutex>

#include <list>

#include "mythpainter.h"
#include "mythimage.h"

class MythOpenGLPainter : public MythPainter
{
  public:
    MythOpenGLPainter();
   ~MythOpenGLPainter();

    virtual QString GetName(void) { return QString("OpenGL"); }
    virtual bool SupportsAnimation(void) { return true; }
    virtual bool SupportsAlpha(void) { return true; }
    virtual bool SupportsClipping(void) { return false; }

    virtual void Begin(QWidget *parent);
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

    virtual MythImage *GetFormatImage();
    virtual void DeleteFormatImage(MythImage *im);

  protected:
    void RemoveImageFromCache(MythImage *im);
    void BindTextureFromCache(MythImage *im, bool alphaonly = false);

    inline int CalcAlpha(int alpha1, int alpha2);

    int NearestGLTextureSize(int v);

    MythImage *GetImageFromString(const QString &msg, int flags, const QRect &r,
                                  const MythFontProperties &font);

    QMap<MythImage *, unsigned int> m_ImageIntMap;
    std::list<MythImage *> m_ImageExpireList;

    QMap<QString, MythImage *> m_StringToImageMap;
    std::list<QString> m_StringExpireList;

    int q_gl_texture;
    bool texture_rects;
    int m_maxTexDim;

    bool init_extensions;
    bool generate_mipmaps;

    std::list<long long> m_textureDeleteList;
    QMutex               m_textureDeleteLock;
};

#endif
