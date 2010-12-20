#ifndef MYTHUI_VIDEO_H_
#define MYTHUI_VIDEO_H_

#include <QString>
#include <QColor>

#include "mythuitype.h"

/**
 * \class MythUIVideo
 *
 * \brief Video widget, displays raw image data
 */
class MPUBLIC MythUIVideo : public MythUIType
{
  public:
    MythUIVideo(MythUIType *parent, const QString &name);
   ~MythUIVideo();

    void UpdateFrame(MythImage *image);
    void UpdateFrame(QPixmap *pixmap);

    QColor GetBackgroundColor(void) { return m_backgroundColor; }

    void Reset(void);
    virtual void Pulse(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    MythImage     *m_image;
    QColor         m_backgroundColor;
};

#endif
