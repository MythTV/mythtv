#ifndef MFDLOGGING_H_
#define MFDLOGGING_H_
/*
	logging.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for logging object of the myth frontend daemon

*/
#include <unistd.h>
#include <iostream>
using namespace std;

#include <qobject.h>
#include <qstring.h>
#include <qfile.h>
#include <qtextstream.h>


class MFDLogger : public QObject
{

    Q_OBJECT

    //
    //  Simple little class to log mtd information
    //  Doesn't do much at the moment, but in place early
    //  so that it will be easy to change logging information
    //  (in transcoding directories, syslog, etc.) later.
    //

  public:
  
    MFDLogger(bool log_stdout, int verbosity_level);
    ~MFDLogger();
    
  public slots:

    void addEntry(const QString &log_entry);
    void addEntry(int verbosity_level, const QString &log_entry);
    void addStartup();
    void addShutdown();

  private:
  
    void  writeString(const QString &log_entry);
    void  writeStampedString(int verbosity_level, const QString &log_entry);
    QFile logging_file;
    bool  log_to_stdout;
    int   verbosity;
};

#endif  // mfdlogging_h_

