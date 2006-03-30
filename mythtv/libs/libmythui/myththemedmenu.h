#ifndef MYTHTHEMEDMENU_H_
#define MYTHTHEMEDMENU_H_

#include "mythscreentype.h"

class MythMainWindow;
class MythThemedMenuPrivate;
class MythThemedMenuState;

class MythThemedMenu : public MythScreenType
{
    Q_OBJECT
  public:
    MythThemedMenu(const char *cdir, const char *menufile,
                   MythScreenStack *parent, const char *name, 
                   bool allowreorder = true, MythThemedMenuState *state = NULL);
   ~MythThemedMenu();

    bool foundTheme(void);

    void setCallback(void (*lcallback)(void *, QString &), void *data);
    void setKillable(void);

    QString getSelection(void);

    void ReloadTheme(void);
    void ReloadExitKey(void);

  protected:
    virtual bool keyPressEvent(QKeyEvent *e);

  private:
    void Init(const char *cdir, const char *menufile);

    MythThemedMenuPrivate *d;
};

#endif
