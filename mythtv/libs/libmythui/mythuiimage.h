#ifndef MYTHUI_IMAGE_H_
#define MYTHUI_IMAGE_H_

#include <qstring.h>

#include "mythuitype.h"
#include "mythimage.h"

class MythUIImage : public MythUIType
{
  public:
    MythUIImage(const QString &filename, MythUIType *parent, const char *name);
    MythUIImage(MythUIType *parent, const char *name);
   ~MythUIImage();

    // doesn't load
    void SetFilename(const QString &filename);
    // load's original
    void ResetFilename();

    void SetImage(const QImage &img);

    void SetSize(int width, int height);
    void SetSkip(int x, int y);

    void Reset(void);
    void Load(void);

    QImage GetImage(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                          int alphaMod);

    void Init(void);

    QString m_Filename;
    QString m_OrigFilename;

    MythImage *m_Image;

    int m_SkipX;
    int m_SkipY;

    int m_ForceW;
    int m_ForceH;
};

#endif
