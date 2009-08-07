#ifndef METAIO_H_
#define METAIO_H_

// QT
#include <QString>

#define MYTH_MUSICBRAINZ_ALBUMARTIST_UUID "89ad4ac3-39f7-470e-963a-56509c546377"

// No need to include all the Metadata stuff just for the abstract pointer....
class Metadata;

class MetaIO
{
  public:
    MetaIO(void);
    virtual ~MetaIO(void);

    /*!
    * \brief Writes metadata back to a file
    *
    * \param mdata A pointer to a Metadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool write(Metadata* mdata) = 0;

    /*!
    * \brief Reads Metadata from a file.
    *
    * \param filename The filename to read metadata from.
    * \returns Metadata pointer or NULL on error
    */
    virtual Metadata* read(QString filename) = 0;

    void readFromFilename(QString filename, QString &artist, QString &album, 
                          QString &title, QString &genre, int &tracknum);

    Metadata* readFromFilename(QString filename, bool blnLength = false);

    void readFromFilename(Metadata *metadata);
    
  protected:

  private:
    virtual int getTrackLength(QString filename) = 0;

    QString mFilename;
    QString mFilenameFormat;
};

#endif

