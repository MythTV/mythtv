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
    virtual int x11ProcessEvent(XEvent *event);
};

#endif // _MYTH_APPLICATION_H_
