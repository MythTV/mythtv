#ifndef MTDLOGGING_H_
#define MTDLOGGING_H_
/*
    logging.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Headers for logging object of the myth transcoding daemon

*/

#include <QObject>
#include <QString>
#include <QFile>


/** \class MTDLogger
 *  \brief Simple little class to log mtd information.
 *
 *  Doesn't do much at the moment, but in place early
 *  so that it will be easy to change logging information
 *  (in transcoding directories, syslog, etc.) later.
 */

class MTDLogger : public QObject
{
    Q_OBJECT

  public:
    MTDLogger(bool log_stdout);
    bool Init(void);
   
  public slots:

    void addEntry(const QString &log_entry);
    void addStartup();
    void addShutdown();
    void socketOpened();
    void socketClosed();

  private:
    ~MTDLogger();
  
  private:
    void writeString(const QString &log_entry);
    void writeStampedString(const QString &log_entry);
    QFile logging_file;
    bool  log_to_stdout;
    
};

#endif  // mtdlogging_h_

