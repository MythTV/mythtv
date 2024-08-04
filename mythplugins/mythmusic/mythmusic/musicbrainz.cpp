#include "musicbrainz.h"
#include "config.h"

// Qt
#include <QObject>
#include <QFile>

// MythTV
#include "libmythbase/mythmiscutil.h"

#ifdef HAVE_MUSICBRAINZ

#include <string>
#include <fstream>

// libdiscid
#include <discid/discid.h>

// libmusicbrainz5
#include "musicbrainz5/Artist.h"
#include "musicbrainz5/ArtistCredit.h"
#include "musicbrainz5/NameCredit.h"
#include "musicbrainz5/Query.h"
#include "musicbrainz5/Disc.h"
#include "musicbrainz5/Medium.h"
#include "musicbrainz5/Release.h"
#include "musicbrainz5/Track.h"
#include "musicbrainz5/TrackList.h"
#include "musicbrainz5/Recording.h"
#include "musicbrainz5/HTTPFetch.h"

// libcoverart
#include "coverart/CoverArt.h"
#include "coverart/HTTPFetch.h"

constexpr auto user_agent = "mythtv";

std::string MusicBrainz::queryDiscId(const std::string &device)
{
    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Query disc id for device %1").arg(QString::fromStdString(device)));
    DiscId *disc = discid_new();
    std::string disc_id;
    if ( discid_read_sparse(disc, device.c_str(), 0) == 0 )
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: %1").arg(discid_get_error_msg(disc)));
    }
    else
    {
        disc_id = discid_get_id(disc);
        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Got disc id %1").arg(QString::fromStdString(disc_id)));
    }
    discid_free(disc);

    return disc_id;
}

/// Compile artist names from artist credits
static std::vector<std::string> queryArtists(const MusicBrainz5::CArtistCredit *artist_credit)
{
    std::vector<std::string> artist_names;
    if (!artist_credit)
    {
        return artist_names;
    }

    std::string joinPhrase;
    for (int a = 0; a < artist_credit->NameCreditList()->NumItems(); ++a)
    {
        auto *nameCredit = artist_credit->NameCreditList()->Item(a);
        auto *artist = nameCredit->Artist();
        if (a == 0)
        {
            joinPhrase = nameCredit->JoinPhrase();
            artist_names.emplace_back(artist->Name());
        }
        else if (!joinPhrase.empty())
        {
            artist_names.back() += joinPhrase + artist->Name();
        }
        else
        {
            artist_names.emplace_back(artist->Name());
        }
    }
    return artist_names;
}

/// Creates single artist string from artist list
static QString artistsToString(const std::vector<std::string> &artists)
{
    QString res;
    for (const auto &artist : artists)
    {
        res += QString(res.isEmpty() ? "%1" : "; %1").arg(artist.c_str());
    }
    return res;
}

/// Log MusicBrainz query errors
static void logError(MusicBrainz5::CQuery &query)
{
    LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: LastResult: %1").arg(query.LastResult()));
    LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: LastHTTPCode: %1").arg(query.LastHTTPCode()));
    LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: LastErrorMessage: '%1'").arg(QString::fromStdString(query.LastErrorMessage())));
}

std::string MusicBrainz::queryRelease(const std::string &discId)
{
    // clear old metadata
    m_tracks.clear();

    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Query metadata for disc id '%1'").arg(QString::fromStdString(discId)));
    MusicBrainz5::CQuery query(user_agent);
    try
    {
        auto discMetadata = query.Query("discid", discId);
        if (discMetadata.Disc() && discMetadata.Disc()->ReleaseList())
        {
            auto *releases = discMetadata.Disc()->ReleaseList();
            LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Found %1 release(s)").arg(releases->NumItems()));
            for (int count = 0; count < releases->NumItems(); ++count)
            {
                auto *basicRelease = releases->Item(count);
                // The releases returned from LookupDiscID don't contain full information
                MusicBrainz5::CQuery::tParamMap params;
                params["inc"]="artists recordings artist-credits discids";
                auto releaseMetadata = query.Query("release", basicRelease->ID(), "", params);
                if (releaseMetadata.Release())
                {
                    auto *fullRelease = releaseMetadata.Release();
                    if (!fullRelease)
                    {
                        continue;
                    }
                    auto media = fullRelease->MediaMatchingDiscID(discId);
                    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Found %1 matching media").arg(media.NumItems()));
                    int artistDiff = 0;
                    for (int m = 0; m < media.NumItems(); ++m)
                    {
                        auto *medium = media.Item(m);
                        if (!medium || !medium->ContainsDiscID(discId))
                        {
                            continue;
                        }
                        std::string albumTitle;
                        if (!medium->Title().empty())
                        {
                            albumTitle = medium->Title();
                        }
                        else if(!fullRelease->Title().empty())
                        {
                            albumTitle = fullRelease->Title();
                        }
                        const auto albumArtists = queryArtists(fullRelease->ArtistCredit());
                        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Release: %1").arg(QString::fromStdString(fullRelease->ID())));
                        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Title: %1").arg(QString::fromStdString(albumTitle)));
                        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Artist: %1").arg(artistsToString(albumArtists)));
                        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Date: %1").arg(QString::fromStdString(fullRelease->Date())));
                        auto *tracks = medium->TrackList();
                        if (tracks)
                        {
                            LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Found %1 track(s)").arg(tracks->NumItems()));
                            for (int t = 0; t < tracks->NumItems(); ++t)
                            {
                                auto *track = tracks->Item(t);
                                if (track && track->Recording())
                                {
                                    auto *recording = track->Recording();
                                    const auto length = std::div(recording->Length() / 1000, 60);
                                    const int minutes = length.quot;
                                    const int seconds = length.rem;
                                    const auto artists = queryArtists(recording->ArtistCredit());
                                    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: %1: %2:%3 - %4 (%5)")
                                        .arg(track->Position())
                                        .arg(minutes, 2).arg(seconds, 2, 10, QChar('0'))
                                        .arg(QString::fromStdString(recording->Title()),
                                             artistsToString(artists)));

                                    // fill metadata
                                    MusicMetadata &metadata = m_tracks[track->Position()];
                                    metadata.setAlbum(QString::fromStdString(albumTitle));
                                    metadata.setTitle(QString::fromStdString(recording->Title()));
                                    metadata.setTrack(track->Position());
                                    metadata.setLength(std::chrono::milliseconds(recording->Length()));
                                    if (albumArtists.size() == 1)
                                    {
                                        metadata.setCompilationArtist(QString::fromStdString(albumArtists[0]));
                                    }
                                    else if(albumArtists.size() > 1)
                                    {
                                        metadata.setCompilationArtist(QObject::tr("Various Artists"));
                                    }
                                    if (artists.size() == 1)
                                    {
                                        metadata.setArtist(QString::fromStdString(artists[0]));
                                    }
                                    else if(artists.size() > 1)
                                    {
                                        metadata.setArtist(QObject::tr("Various Artists"));
                                    }
                                    if (metadata.CompilationArtist() != metadata.Artist())
                                    {
                                        artistDiff++;
                                    }
                                    metadata.setYear(QDate::fromString(QString::fromStdString(fullRelease->Date()), Qt::ISODate).year());
                                }
                            }
                        }
                    }
                    // Set compilation flag if album artist differs from track artists
                    // as there might be some tracks featuring guest artists we only set
                    // the compilation flag if at least half of the track artists differ
                    setCompilationFlag(artistDiff > m_tracks.count() / 2);

                    return fullRelease->ID();
                }
            }
        }
    }
    catch (MusicBrainz5::CConnectionError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Connection Exception: '%1'").arg(error.what()));
        logError(query);
    }
    catch (MusicBrainz5::CTimeoutError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Timeout Exception: '%1'").arg(error.what()));
        logError(query);
    }
    catch (MusicBrainz5::CAuthenticationError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Authentication Exception: '%1'").arg(error.what()));
        logError(query);
    }
    catch (MusicBrainz5::CFetchError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Fetch Exception: '%1'").arg(error.what()));
        logError(query);
    }
    catch (MusicBrainz5::CRequestError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Request Exception: '%1'").arg(error.what()));
        logError(query);
    }
    catch (MusicBrainz5::CResourceNotFoundError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: ResourceNotFound Exception: '%1'").arg(error.what()));
        logError(query);
    }

    return {};
}

void MusicBrainz::setCompilationFlag(bool isCompilation)
{
    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Setting compilation flag: %1").arg(isCompilation));
    for (auto &metadata : m_tracks)
    {
        metadata.setCompilation(isCompilation);
    }
}

static void logError(CoverArtArchive::CCoverArt &coverArt)
{
    LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: LastResult: %1").arg(coverArt.LastResult()));
    LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: LastHTTPCode: %1").arg(coverArt.LastHTTPCode()));
    LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: LastErrorMessage: '%1'").arg(QString::fromStdString(coverArt.LastErrorMessage())));
}

QString MusicBrainz::queryCoverart(const std::string &releaseId)
{
    const QString fileName = QString("musicbrainz-%1-front.jpg").arg(releaseId.c_str());
    QString filePath = QDir::temp().absoluteFilePath(fileName);
    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Check if coverart file exists for release '%1'").arg(QString::fromStdString(releaseId)));
    if (QDir::temp().exists(fileName))
    {
        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Cover art file '%1' exist already").arg(filePath));
        return filePath;
    }

    LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Query cover art for release '%1'").arg(QString::fromStdString(releaseId)));
    CoverArtArchive::CCoverArt coverArt(user_agent);
    try
    {
        std::vector<unsigned char> imageData = coverArt.FetchFront(releaseId);
        if (!imageData.empty())
        {
            LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Saving front coverart to '%1'").arg(filePath));

            QFile coverArtFile(filePath);
            if (!coverArtFile.open(QIODevice::WriteOnly))
            {
                LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Unable to open temporary file '%1'").arg(filePath));
                return {};
            }

            const auto coverArtBytes = static_cast<qint64>(imageData.size());
            const auto writtenBytes = coverArtFile.write(reinterpret_cast<const char*>(imageData.data()), coverArtBytes);
            coverArtFile.close();
            if (writtenBytes != coverArtBytes)
            {
                LOG(VB_MEDIA, LOG_ERR, QString("ERROR musicbrainz: Could not write coverart data to file '%1'").arg(filePath));
                return {};
            }

            return filePath;
        }
    }
    catch (CoverArtArchive::CConnectionError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Connection Exception: '%1'").arg(error.what()));
        logError(coverArt);
    }
    catch (CoverArtArchive::CTimeoutError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Timeout Exception: '%1'").arg(error.what()));
        logError(coverArt);
    }
    catch (CoverArtArchive::CAuthenticationError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Authentication Exception: '%1'").arg(error.what()));
        logError(coverArt);
    }
    catch (CoverArtArchive::CFetchError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Fetch Exception: '%1'").arg(error.what()));
        logError(coverArt);
    }
    catch (CoverArtArchive::CRequestError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: Request Exception: '%1'").arg(error.what()));
        logError(coverArt);
    }
    catch (CoverArtArchive::CResourceNotFoundError& error)
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: ResourceNotFound Exception: '%1'").arg(error.what()));
        logError(coverArt);
    }

    return {};
}

#endif // HAVE_MUSICBRAINZ

bool MusicBrainz::queryForDevice(const QString &deviceName)
{
#ifdef HAVE_MUSICBRAINZ
    const auto discId = queryDiscId(deviceName.toStdString());
    if (discId.empty())
    {
        reset();
        return false;
    }
    if (discId == m_discId)
    {
        // already queried
        LOG(VB_MEDIA, LOG_DEBUG, QString("musicbrainz: Metadata for disc %1 already present").arg(QString::fromStdString(m_discId)));
        return true;
    }

    // new disc id, reset existing data
    reset();

    const auto releaseId = queryRelease(discId);
    if (releaseId.empty())
    {
        return false;
    }
    const auto covertArtFileName = queryCoverart(releaseId);
    if (!covertArtFileName.isEmpty())
    {
        m_albumArt.m_filename = covertArtFileName;
        m_albumArt.m_imageType = IT_FRONTCOVER;
    }
    m_discId = discId;

    return true;
#else
    return false;
#endif
}

bool MusicBrainz::hasMetadata(int track) const
{
    return m_tracks.find(track) != m_tracks.end();
}

MusicMetadata *MusicBrainz::getMetadata(int track) const
{
    auto it = m_tracks.find(track);
    if (it == m_tracks.end())
    {
        LOG(VB_MEDIA, LOG_ERR, QString("musicbrainz: No metadata for track %1").arg(track));
        return nullptr;
    }
    auto *metadata = new MusicMetadata(it.value());
    if (!m_albumArt.m_filename.isEmpty())
    {
        metadata->getAlbumArtImages()->addImage(&m_albumArt);
    }
    return metadata;
}

void MusicBrainz::reset()
{
    LOG(VB_MEDIA, LOG_DEBUG, "musicbrainz: Reset metadata");
    m_tracks.clear();
    m_albumArt = AlbumArtImage();
}

