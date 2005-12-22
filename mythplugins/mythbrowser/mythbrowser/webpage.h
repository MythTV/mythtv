#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <kapplication.h>
#include <kmainwindow.h>
#include <kurl.h>
#include <kparts/browserextension.h>


class KHTMLPart;

class WebPage : public QWidget
{
  Q_OBJECT

protected:
  virtual bool eventFilter(QObject* object, QEvent* event);

public:
  WebPage (const char *location, int zoom, WFlags flags);
  WebPage (const char *location, const KParts::URLArgs &args, int zoom, WFlags flags);
  ~WebPage();
  int zoomFactor;
  KHTMLPart *browser;

public slots:
  void openURLRequest(const KURL &url, const KParts::URLArgs &args);
  void createNewWindow(const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&);
  void openURL(const QString &url);
  void zoomIn();
  void zoomOut();
  void started(KIO::Job *);    // Started to download the requested URL
  void completed();            // Requested URL download completed
  void timerTimeout();
  
signals:
  void newUrlRequested(const KURL &url, const KParts::URLArgs &args);
  void newWindowRequested(const KURL &, const KParts::URLArgs &, 
                                  const KParts::WindowArgs &, KParts::ReadOnlyPart *&);
private:
  QString sUrl;
};

#endif // WEBPAGE_H
