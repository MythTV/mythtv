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

    /*!
    * \brief Reads Metadata based on the folder/filename.
    *
    * \param filename The filename to try and determine metadata for.
    * \returns artist, album, title, genre & tracknum in parameters.
    */
    void readFromFilename(const QString &filename, QString &artist,
                          QString &album, QString &title, QString &genre,
                          int &tracknum);

    /*!
    * \brief Reads Metadata based on the folder/filename.
    *
    * \param filename The filename to try and determine metadata for.
    * \returns Metadata Pointer, or NULL on error.
    */
    Metadata* readFromFilename(const QString &filename, bool blnLength = false);

    void readFromFilename(Metadata *metadata);

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
    * \returns an instance of \p Metadata owned by the caller
    */
    static Metadata *readMetadata(const QString &filename);

    /*!
    * \brief Get the metadata for \p filename
    *
    * First tries to read the metadata from the database. If there
    * is no database entry, it'll call \p MetaIO::readMetadata.
    *
    * \param filename The filename to get metadata for.
    * \returns an instance of \p Metadata owned by the caller
    */
    static Metadata *getMetadata(const QString &filename);

  protected:

  private:
    virtual int getTrackLength(const QString &filename) = 0;

    QString mFilename;
    QString mFilenameFormat;
};

#endif

