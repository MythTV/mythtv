#ifndef TEST1_H_
#define TEST1_H_

#include "mythscreentype.h"

class TestScreen1 : public MythScreenType
{
  public:
    TestScreen1(MythScreenStack *parent, const char *name);

    virtual bool keyPressEvent(QKeyEvent *e);

  protected:
    virtual void customEvent(QCustomEvent *e);

  private:
    void Launch1();
    void Launch2();
    void Launch3();
    void Launch4();
    void Launch5();
    void Launch6();
    void Launch7();
    void Launch8();
    void LaunchMenu();
};

class TestMove : public MythUIType
{
    Q_OBJECT
  public:
    TestMove(MythUIType *parent, const char *name);

  public slots:
    void moveDone();

  protected:
    int position;
};

#endif
