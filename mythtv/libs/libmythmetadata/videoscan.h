#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <QObject> // for moc

#include "mythexp.h"

class QStringList;

class MPUBLIC VideoScanner : public QObject
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
