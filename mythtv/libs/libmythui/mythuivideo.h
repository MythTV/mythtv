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
class MUI_PUBLIC MythUIVideo : public MythUIType
{
  public:
    MythUIVideo(MythUIType *parent, const QString &name);
   ~MythUIVideo() override;

    void UpdateFrame(MythImage *image);
    void UpdateFrame(QPixmap *pixmap);

    QColor GetBackgroundColor(void) { return m_backgroundColor; }

    void Reset(void) override; // MythUIType
    void Pulse(void) override; // MythUIType

  protected:
    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType

    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

    MythImage     *m_image           {nullptr};
    QColor         m_backgroundColor {Qt::black};
};

#endif
