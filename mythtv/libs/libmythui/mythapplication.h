// -*- Mode: c++ -*-
#ifndef _MYTH_APPLICATION_H_
#define _MYTH_APPLICATION_H_

#include <QApplication>

#include "mythexp.h"

class MPUBLIC MythApplication : public QApplication
{
  public:
    MythApplication(int &argc, char **argv) : QApplication(argc, argv) { }
    MythApplication(int &argc, char **argv, bool GUIenabled) :
        QApplication(argc, argv, GUIenabled) { }
#ifdef USING_X11
    virtual int x11ProcessEvent(XEvent *event);
#endif // USING_X11
};

#endif // _MYTH_APPLICATION_H_
