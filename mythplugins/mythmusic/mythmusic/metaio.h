#ifndef METAIO_H_
#define METAIO_H_

#include <qregexp.h>

// No need to include all the Metadata stuff just for the abstract pointer....
class Metadata;

class MetaIO
{
public:
    MetaIO(QString fileExtension);
    virtual ~MetaIO(void);
    
    virtual bool write(Metadata* mdata, bool exclusive = false) = 0;
    virtual Metadata* read(QString filename) = 0;

    void readFromFilename(QString filename, QString &artist, QString &album, 
                          QString &title, QString &genre, int &tracknum);

    Metadata* readFromFilename(QString filename, bool blnLength = false);

protected:

private:
    virtual int getTrackLength(QString filename) = 0;

    QString mFilename;

    QString mFileExtension;

    QString mFilenameFormat;
};

#endif

