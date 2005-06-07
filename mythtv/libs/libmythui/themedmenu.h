#ifndef THEMEDMENU_H_
#define THEMEDMENU_H_

#include "mythscreentype.h"

class MythMainWindow;
class ThemedMenuPrivate;
class ThemedMenuState;

class ThemedMenu : public MythScreenType
{
    Q_OBJECT
  public:
    ThemedMenu(const char *cdir, const char *menufile,
               MythScreenStack *parent, const char *name, 
               bool allowreorder = true, ThemedMenuState *state = NULL);
   ~ThemedMenu();

    bool foundTheme(void);

    void setCallback(void (*lcallback)(void *, QString &), void *data);
    void setKillable(void);

    QString getSelection(void);

    void ReloadTheme(void);
    void ReloadExitKey(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);
    virtual bool keyPressEvent(QKeyEvent *e);

  private:
    void Init(const char *cdir, const char *menufile);

    ThemedMenuPrivate *d;
};

#endif
