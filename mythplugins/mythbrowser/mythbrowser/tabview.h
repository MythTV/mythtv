#ifndef TABVIEW_H
#define TABVIEW_H

#include "webpage.h"

#include <kapplication.h>
#include <kmainwindow.h>
#include <qtabwidget.h>
#include <qsqldatabase.h>

#include "mythtv/mythdialogs.h"

class MyMythPopupBox : public MythPopupBox
{
    Q_OBJECT

public:
    MyMythPopupBox(MythMainWindow *parent, const char *name = 0);
//    ~MyMythPopupBox();
};

class TabView : public MythMainWindow
{
  Q_OBJECT

protected:
  virtual bool eventFilter(QObject* object, QEvent* event);

public:
  TabView(QSqlDatabase *db, QStringList urls,
          int zoom, int width, int height, WFlags flags);
  ~TabView();

signals:
  void menuPressed();
  void closeMenu();

public slots:
  void changeTitle(QString title);

private slots:
  void openMenu();
  void actionBack();
  void actionNextTab();
  void actionPrevTab();
  void actionMouseEmulation();
//  void actionDoLeftClick();
  void actionSTOP();
  void actionZoomOut();
  void actionZoomIn();
  void cancelMenu();
  void actionAddBookmark();
  void finishAddBookmark(const char* group, const char* desc, const char* url);

private:
  int z,w,h;
  WFlags f;
  QSqlDatabase       *myDb;

  int mouseEmulation, saveMouseEmulation;

  QStringList urls;
  QTabWidget *mytab;

  QCursor *mouse;

  bool menuIsOpen;
  MyMythPopupBox *menu;

  QWidget *hadFocus;
};

class PopupBox : public QDialog
{
    Q_OBJECT

protected:
//  virtual bool eventFilter(QObject* object, QEvent* event);

public:
    PopupBox(QWidget *parent, QString defltUrl);
    ~PopupBox();

signals:
    void finished(const char* group, const char* desc,const char* url);
//    void cancelMenu();

private slots:
    void slotOkClicked();

private:
    QLineEdit* group;
    QLineEdit* desc;
    QLineEdit* url;
};

#endif // TABVIEW_H
