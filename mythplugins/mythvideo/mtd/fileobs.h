#ifndef FILEOBS_H_
#define FILEOBS_H_
/*
    fileobs.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Headers for file objects that know
    how to do clever things

*/

#include <QFile>
#include <QStringList>

class RipFile
{

  public:
  
    RipFile(const QString &a_base, const QString &an_extension,
            bool auto_remove_bad);
    ~RipFile();
    
    bool    open(const QIODevice::OpenMode &mode, bool multiple_files);
    QStringList close();
    void    remove();
    QString name(void) const;
    bool    writeBlocks(unsigned char *the_data, int how_much);

  private:
    QString         base_name;
    QString         extension;
    int             filesize;
    int             active_file;
    int             bytes_written;
    QIODevice::OpenMode access_mode;
    QList<QFile*>   files;
    bool            use_multiple_files;
    bool            auto_remove_bad_rips;
};

#endif  // fileobs_h_

