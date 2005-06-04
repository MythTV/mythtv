#ifndef MYTHUI_STATEIMAGE_H_
#define MYTHUI_STATEIMAGE_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qvaluevector.h>

#include "mythuitype.h"
#include "mythimage.h"

// Image class that displays one of an array of images (either predefined states
// or by typename).  Displays nothing if told to display a non-existant state
class MythUIStateImage : public MythUIType
{
  public:
    enum StateType { None = 0, Off, Half, Full }; // Can be used for tri-state checks, two state toggles, etc.

    MythUIStateImage(MythUIType *parent, const char *name);
   ~MythUIStateImage();

    // These passed in images are owned or not, depending on the autodelete
    bool AddImage(const QString &name, MythImage *image);
    bool AddImage(StateType type, MythImage *image);

    void DisplayImage(const QString &name);
    void DisplayImage(StateType type);

    void ClearImages();

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod);

    void ClearMaps();

    QMap<QString, MythImage *> m_ImagesByName;
    QMap<int, MythImage *> m_ImagesByState;

    MythImage *m_CurrentImage;
};

#endif
