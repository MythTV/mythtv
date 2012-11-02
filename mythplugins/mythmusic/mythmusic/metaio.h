#ifndef METAIO_H_
#define METAIO_H_

// QT
#include <QString>

// MythMusic
#include "metadata.h"

#define MYTH_MUSICBRAINZ_ALBUMARTIST_UUID "89ad4ac3-39f7-470e-963a-56509c546377"

class MetaIO
{
  public:
    MetaIO(void);
    virtual ~MetaIO(void);

    /*!
    * \brief Writes all metadata back to a file
    *
    * \param mdata A pointer to a Metadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool write(const Metadata* mdata) = 0;

    /*!
    * \brief Writes rating and playcount back to a file
    *
    * \param mdata A pointer to a Metadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool writeVolatileMetadata(const Metadata* mdata)
    {
        (void)mdata;
        return false;
    }

    /*!
    * \brief Reads Metadata from a file.
    *
    * \param filename The filename to read metadata from.
    * \returns Metadata pointer or NULL on error
    */
    virtual Metadata* read(const QString &filename) = 0;

    /*!
    * \brief Does the tag support embedded cover art.
    *
    * \returns true if reading/writing embedded images are supported
    */
    virtual bool supportsEmbeddedImages(void)
    {
        return false;
    }

    /*!
    * \brief Reads the list of embedded images in the tag
    *
    * \param filename The filename to read images from.
    * \returns the list of embedded images
    */
    virtual AlbumArtList getAlbumArtList(const QString &filename)
    {
        (void)filename;
        return AlbumArtList();
    }

    virtual bool writeAlbumArt(const QString &filename,
                               const AlbumArtImage *albumart)
    {
        (void)filename;
        (void)albumart;
        return false;
    }

    virtual bool removeAlbumArt(const QString &filename,
                                const AlbumArtImage *albumart)
    {
        (void)filename;
        (void)albumart;
        return false;
    }

    virtual bool changeImageType(const QString &filename,
                                 const AlbumArtImage *albumart,
                                 ImageType newType)
    {
        (void)filename;
        (void)albumart;
        (void)newType;
        return false;
    }

    virtual QImage *getAlbumArt(const QString &filename, ImageType type)
    {
        (void)filename;
        (void)type;
        return NULL;
    }

    void readFromFilename(const QString &filename, QString &artist,
                          QString &album, QString &title, QString &genre,
                          int &tracknum);

    Metadata* readFromFilename(const QString &filename, bool blnLength = false);

    void readFromFilename(Metadata *metadata);

    virtual bool TagExists(const QString &filename)
    { 
        (void)filename;
        return false;
    }
    
  protected:

  private:
    virtual int getTrackLength(const QString &filename) = 0;

    QString mFilename;
    QString mFilenameFormat;
};

#endif

