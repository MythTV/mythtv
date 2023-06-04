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

static constexpr const char* MYTH_MUSICBRAINZ_ALBUMARTIST_UUID { "89ad4ac3-39f7-470e-963a-56509c546377" };

class META_PUBLIC MetaIO
{
  public:
    MetaIO(void)
        : m_filenameFormat(gCoreContext->GetSetting("NonID3FileNameFormat").toUpper()) {};
    virtual ~MetaIO(void) = default;

    /*!
    * \brief Writes all metadata back to a file
    *
    * \param filename The filename to write metadata to
    * \param mdata A pointer to a MusicMetadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool write(const QString &filename, MusicMetadata* mdata) = 0;

    /*!
    * \brief Writes rating and playcount back to a file
    * \param filename The filename to write metadata to
    * \param mdata A pointer to a MusicMetadata object
    * \returns Boolean to indicate success/failure.
    */
    virtual bool writeVolatileMetadata([[maybe_unused]] const QString & filename,
                                       [[maybe_unused]] MusicMetadata* mdata)
    {
        return false;
    }

    /*!
    * \brief Reads MusicMetadata from a file.
    *
    * \param filename The filename to read metadata from.
    * \returns MusicMetadata pointer or nullptr on error
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
    virtual AlbumArtList getAlbumArtList([[maybe_unused]] const QString &filename)
    {
        return {};
    }

    virtual bool writeAlbumArt([[maybe_unused]] const QString &filename,
                               [[maybe_unused]] const AlbumArtImage *albumart)
    {
        return false;
    }

    virtual bool removeAlbumArt([[maybe_unused]] const QString &filename,
                                [[maybe_unused]] const AlbumArtImage *albumart)
    {
        return false;
    }

    virtual bool changeImageType([[maybe_unused]] const QString &filename,
                                 [[maybe_unused]] const AlbumArtImage *albumart,
                                 [[maybe_unused]] ImageType newType)
    {
        return false;
    }

    virtual QImage *getAlbumArt([[maybe_unused]] const QString &filename,
                                [[maybe_unused]] ImageType type)
    {
        return nullptr;
    }

    /*!
    * \brief Reads MusicMetadata based on the folder/filename.
    *
    * \param[in]  filename The filename to try and determine metadata for.
    * \param[out] artist
    * \param[out] album
    * \param[out] title
    * \param[out] genre
    * \param[out] tracknum
    */
    void readFromFilename(const QString &filename, QString &artist,
                          QString &album, QString &title, QString &genre,
                          int &tracknum);

    /*!
    * \brief Reads MusicMetadata based on the folder/filename.
    *
    * \param filename The filename to try and determine metadata for.
    * \param blnLength If true, read the file length as well.
    * \returns MusicMetadata Pointer, or nullptr on error.
    */
    MusicMetadata* readFromFilename(const QString &filename, bool blnLength = false);

    void readFromFilename(MusicMetadata *metadata);

    virtual bool TagExists([[maybe_unused]] const QString &filename)
    { 
        return false;
    }

    /*!
    * \brief Finds an appropriate tagger for the given file
    *
    * \param filename The filename to find a tagger for.
    * \returns MetaIO Pointer, or nullptr if non found.
    *
    * \note The caller is responsible for freeing it when no longer required
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

    static const QString kValidFileExtensions;

  protected:
    void saveTimeStamps(void);
    void restoreTimeStamps(void);

    virtual std::chrono::milliseconds getTrackLength(const QString &filename) = 0;

    QString m_filename;
    QString m_filenameFormat;

    struct stat m_fileinfo {};
};

#endif

