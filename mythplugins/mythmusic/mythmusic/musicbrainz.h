#ifndef MUSICBRAINZ_H
#define MUSICBRAINZ_H

#include "config.h"

// Qt
#include <QString>
#include <QMap>

// MythTV
#include <libmythmetadata/musicmetadata.h>

class MusicBrainz
{
public:
    /**
     * Query music metadata using disc id of specified device
     *
     * @param [in] deviceName name of the CD device to query metadata for
     * @return true if query was successful, false otherwise
     */
    bool queryForDevice(const QString &deviceName);

    /**
     * Checks if metadata for given track exists
     *
     * @param track [in] track number to check metadata for
     * @return true if metadata was found, false otherwise
     */
    bool hasMetadata(int track) const;

    /**
     * Creates and return metadata for specified track
     *
     * @param [in] track the track number for which to return the metadata
     * @return pointer to newly created metadata object, nullptr if no metadata for this track exists
     */
    MusicMetadata *getMetadata(int track) const;

    /**
     * Reset last queried metadata
     */
    void reset();
private:

    /**
     * Sets compilation flag for all metadata
     */
    void setCompilationFlag(bool isCompilation);

#ifdef HAVE_MUSICBRAINZ

    /// Query disc id for specified device
    static std::string queryDiscId(const std::string &device);

    /// Query release id and release metadata
    std::string queryRelease(const std::string &discId);

    /// Query coverart for given release id
    static QString queryCoverart(const std::string &releaseId);

    std::string m_discId; ///< disc id corresponding to current metadata

#endif // HAVE_MUSICBRAINZ

    QMap<int, MusicMetadata> m_tracks;
    AlbumArtImage m_albumArt;
};

#endif // MUSICBRAINZ_H
