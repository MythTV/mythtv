#ifndef METAIO_H_
#define METAIO_H_

// POSIX C headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// QT
#include <QString>

// libmythmetadata
#include "musicmetadata.h"

#define MYTH_MUSICBRAINZ_ALBUMARTIST_UUID "89ad4ac3-39f7-470e-963a-56509c546377"

class META_PUBLIC MetaIO
{
  public:
    MetaIO(void);
    virtual ~MetaIO(void);

    /*!
    * \brief Writes all metadata back to a file
    *
    * \param mdata A pointer to a MusicMetadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool write(const MusicMetadata* mdata) = 0;

    /*!
    * \brief Writes rating and playcount back to a file
    *
    * \param mdata A pointer to a MusicMetadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool writeVolatileMetadata(const MusicMetadata* mdata)
    {
        (void)mdata;
        return false;
    }

    /*!
    * \brief Reads MusicMetadata from a file.
    *
    * \param filename The filename to read metadata from.
    * \returns MusicMetadata pointer or NULL on error
    */
    virtual MusicMetadata* read(const QString &filename) = 0;

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

    /*!
    * \brief Reads MusicMetadata based on the folder/filename.
    *
    * \param filename The filename to try and determine metadata for.
    * \returns artist, album, title, genre & tracknum in parameters.
    */
    void readFromFilename(const QString &filename, QString &artist,
                          QString &album, QString &title, QString &genre,
                          int &tracknum);

    /*!
    * \brief Reads MusicMetadata based on the folder/filename.
    *
    * \param filename The filename to try and determine metadata for.
    * \returns MusicMetadata Pointer, or NULL on error.
    */
    MusicMetadata* readFromFilename(const QString &filename, bool blnLength = false);

    void readFromFilename(MusicMetadata *metadata);

    virtual bool TagExists(const QString &filename)
    { 
        (void)filename;
        return false;
    }

    /*!
    * \brief Finds an appropriate tagger for the given file
    *
    * \param filename The filename to find a tagger for.
    * \returns MetaIO Pointer, or NULL if non found.
    *
    * \Note The caller is responsible for freeing it when no longer required
    */

    static MetaIO *createTagger(const QString &filename);

    /*!
    * \brief Read the metadata from \p filename directly.
    *
    * Creates a \p MetaIO object using \p MetaIO::createTagger and uses
    * the MetaIO object to read the metadata.
    * \param filename The filename to read metadata from.
    * \returns an instance of \p MusicMetadata owned by the caller
    */
    static MusicMetadata *readMetadata(const QString &filename);

    /*!
    * \brief Get the metadata for \p filename
    *
    * First tries to read the metadata from the database. If there
    * is no database entry, it'll call \p MetaIO::readMetadata.
    *
    * \param filename The filename to get metadata for.
    * \returns an instance of \p MusicMetadata owned by the caller
    */
    static MusicMetadata *getMetadata(const QString &filename);

    static const QString ValidFileExtensions;

  protected:
    void saveTimeStamps(void);
    void restoreTimeStamps(void);

    virtual int getTrackLength(const QString &filename) = 0;

    QString m_filename;
    QString m_filenameFormat;

    struct stat m_fileinfo;
};

#endif

