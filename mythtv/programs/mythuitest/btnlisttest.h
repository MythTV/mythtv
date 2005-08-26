#ifndef BTNLISTTEST_H
#define BTNLISTTEST_H

#include <qobject.h>

#include "mythscreentype.h"

class MythListButton;

class TestWindow : public MythScreenType
{

 public:

    static const int pad = 60;
    static const int listHeight = 200;

    TestWindow(MythScreenStack *parent);
    virtual ~TestWindow(void);
    virtual bool keyPressEvent(QKeyEvent *e);

 protected:

    void setupHList(void);
    void setupVList(void);
    void toggleActive(void);

 private:

    int listWidth;
    MythListButton *vbuttons;
    MythListButton *hbuttons;
    MythListButton *focused;
};

#endif /* BTNLISTTEST_H */
