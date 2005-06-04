#ifndef MYTHIMAGE_H_
#define MYTHIMAGE_H_

// Base class, inherited by painter-specific classes.

#include "mythpainter.h"

#include <qimage.h>

class QPixmap;

class MythImage : public QImage
{
  public:
    MythImage(MythPainter *parent);

    void UpRef(void);
    bool DownRef(void);

    virtual void SetChanged(bool change = true) { m_Changed = change; }
    bool IsChanged() { return m_Changed; }

    void Assign(const QImage &img);
    void Assign(const QPixmap &pix);

    // *NOTE* *DELETES* img!
    static MythImage *FromQImage(QImage **img);

    bool Load(const QString &filename);

  protected:
    virtual ~MythImage();

    bool m_Changed;
    MythPainter *m_Parent;

    int m_RefCount;
};

#endif

