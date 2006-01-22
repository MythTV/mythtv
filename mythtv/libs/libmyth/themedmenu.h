#ifndef THEMEDMENU_H_
#define THEMEDMENU_H_

#include <qvaluelist.h>
#include <qdom.h>
#include <qmap.h>

#include "mythdialogs.h"

#include <vector>

using namespace std;

class ThemedMenuPrivate;

class ThemedMenu : public MythDialog
{
    Q_OBJECT
  public:
    ThemedMenu(const char *cdir, const char *menufile, 
               MythMainWindow *parent, const char *name = 0);
    ThemedMenu(const char *cdir, const char *menufile,
               MythMainWindow *parent, bool allowreorder = true);
   ~ThemedMenu();

    bool foundTheme(void);

    void setCallback(void (*lcallback)(void *, QString &), void *data);
    void setKillable(void);

    QString getSelection(void);

    void ReloadTheme(void);
    void ReloadExitKey(void);

    void gotoMainMenu(void);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:
    void Init(const char *cdir, const char *menufile);

    ThemedMenuPrivate *d;
};

#endif
