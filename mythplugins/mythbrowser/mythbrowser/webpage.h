#ifndef WEBPAGE_H
#define WEBPAGE_H

#define DEBUG             0

#include <qvaluestack.h>
#include <qvgroupbox.h>
#include <qprogressbar.h> 

#include <kapplication.h>
#include <kmainwindow.h>
#include <kurl.h>
#include <kparts/browserextension.h>

#include "mythtv/mythdialogs.h"


class KHTMLPart;

class WebPage : public QWidget
{
  Q_OBJECT

protected:
  virtual bool eventFilter(QObject* object, QEvent* event);

public:
  WebPage (const char *location, int zoom, int width, int height, WFlags flags);
  KHTMLPart *browser;

public slots:
  void openURLRequest(const KURL &url, const KParts::URLArgs &args);
   void back();
  void zoomIn();
  void zoomOut();
  void click(QPoint p);

signals:
  void changeTitle(QString);

//  void started(KIO::Job *); // Started to download the requested URL
//  void completed();            // Requested URL download completed
//  void percent(KIO::Job *, unsigned long);

private:
  int zoomFactor;
  QValueStack<QString> urlHistory;
//  QPtrStack<const KParts::URLArgs*> argHistory;
//  MythProgressDialog *progressBar;
};

#endif // WEBPAGE_H
