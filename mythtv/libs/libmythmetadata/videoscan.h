#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <QObject> // for moc

#include "mythmetaexp.h"

class QStringList;

class META_PUBLIC VideoScanner : public QObject
{
    Q_OBJECT

  public:
    VideoScanner();
    ~VideoScanner();

    void doScan(const QStringList &dirs);
    void doScanAll(void);

  signals:
    void finished(bool);

  public slots:
    void finishedScan();

  private:
    class VideoScannerThread *m_scanThread;
    bool                      m_cancel;
};

#endif
