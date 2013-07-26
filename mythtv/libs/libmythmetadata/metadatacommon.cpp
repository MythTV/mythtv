// Qt headers
#include <QLocale>
#include <QCoreApplication>

#include "rssparse.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "metadatacommon.h"
#include "mythlogging.h"
#include "mythlocale.h"
#include "mythmiscutil.h"

// null constructor
MetadataLookup::MetadataLookup(void) :
    m_type(kMetadataVideo),
    m_subtype(kUnknownVideo),
    m_data(),
    m_step(kLookupSearch),
    m_automatic(false),
    m_handleimages(false),
    m_allowoverwrites(false),
    m_allowgeneric(false),
    m_dvdorder(false),
    m_host(),
    m_filename(),
    m_title(),
    m_network(),
    m_status(),
    m_categories(),
    m_userrating(0),
    m_ratingcount(0),
    m_language(),
    m_subtitle(),
    m_tagline(),
    m_description(),
    m_season(0),
    m_episode(0),
    m_chanid(0),
    m_channum(),
    m_chansign(),
    m_channame(),
    m_chanplaybackfilters(),
    m_recgroup(),
    m_playgroup(),
    m_seriesid(),
    m_programid(),
    m_storagegroup(),
    m_startts(),
    m_endts(),
    m_recstartts(),
    m_recendts(),
    m_programflags(0),
    m_audioproperties(0),
    m_videoproperties(0),
    m_subtitletype(0),
    m_certification(),
    m_countries(),
    m_popularity(0),
    m_budget(0),
    m_revenue(0),
    m_album(),
    m_tracknum(0),
    m_system(),
    m_year(0),
    m_releasedate(),
    m_lastupdated(),
    m_runtime(0),
    m_runtimesecs(0),
    m_inetref(),
    m_collectionref(),
    m_tmsref(),
    m_imdb(),
    m_people(),
    m_studios(),
    m_homepage(),
    m_trailerURL(),
    m_artwork(),
    m_downloads()
{
}

// full constructor
MetadataLookup::MetadataLookup(
    MetadataType type,
    LookupType subtype,
    QVariant data,
    LookupStep step,
    bool automatic,
    bool handleimages,
    bool allowoverwrites,
    bool allowgeneric,
    bool preferdvdorder,
    const QString &host,
    const QString &filename,
    const QString &title,
    const QString &network,
    const QString &status,
    const QStringList &categories,
    const float userrating,
    uint ratingcount,
    const QString &language,
    const QString &subtitle,
    const QString &tagline,
    const QString &description,
    uint season,
    uint episode,
    uint chanid,
    const QString &channum,
    const QString &chansign,
    const QString &channame,
    const QString &chanplaybackfilters,
    const QString &recgroup,
    const QString &playgroup,
    const QString &seriesid,
    const QString &programid,
    const QString &storagegroup,
    const QDateTime startts,
    const QDateTime endts,
    const QDateTime recstartts,
    const QDateTime recendts,
    uint programflags,
    uint audioproperties,
    uint videoproperties,
    uint subtitletype,
    const QString &certification,
    const QStringList &countries,
    const uint popularity,
    const uint budget,
    const uint revenue,
    const QString &album,
    uint tracknum,
    const QString &system,
    const uint year,
    const QDate releasedate,
    const QDateTime lastupdated,
    const uint runtime,
    const uint runtimesecs,
    const QString &inetref,
    const QString &collectionref,
    const QString &tmsref,
    const QString &imdb,
    const PeopleMap people,
    const QStringList &studios,
    const QString &homepage,
    const QString &trailerURL,
    const ArtworkMap artwork,
    DownloadMap downloads) :

    m_type(type),
    m_subtype(subtype),
    m_data(data),
    m_step(step),
    m_automatic(automatic),
    m_handleimages(handleimages),
    m_allowoverwrites(allowoverwrites),
    m_allowgeneric(allowgeneric),
    m_dvdorder(preferdvdorder),
    m_host(host),
    m_filename(filename),
    m_title(title),
    m_network(network),
    m_status(status),
    m_categories(categories),
    m_userrating(userrating),
    m_ratingcount(ratingcount),
    m_language(language),
    m_subtitle(subtitle),
    m_tagline(tagline),
    m_description(description),
    m_season(season),
    m_episode(episode),
    m_chanid(chanid),
    m_channum(channum),
    m_chansign(chansign),
    m_channame(channame),
    m_chanplaybackfilters(chanplaybackfilters),
    m_recgroup(recgroup),
    m_playgroup(playgroup),
    m_seriesid(seriesid),
    m_programid(programid),
    m_storagegroup(storagegroup),
    m_startts(startts),
    m_endts(endts),
    m_recstartts(recstartts),
    m_recendts(recendts),
    m_programflags(programflags),
    m_audioproperties(audioproperties),
    m_videoproperties(videoproperties),
    m_subtitletype(subtitletype),
    m_certification(certification),
    m_countries(countries),
    m_popularity(popularity),
    m_budget(budget),
    m_revenue(revenue),
    m_album(album),
    m_tracknum(tracknum),
    m_system(system),
    m_year(year),
    m_releasedate(releasedate),
    m_lastupdated(lastupdated),
    m_runtime(runtime),
    m_runtimesecs(runtimesecs),
    m_inetref(inetref),
    m_collectionref(collectionref),
    m_tmsref(tmsref),
    m_imdb(imdb),
    m_people(people),
    m_studios(studios),
    m_homepage(homepage),
    m_trailerURL(trailerURL),
    m_artwork(artwork),
    m_downloads(downloads)
{
}

// ProgramInfo-style constructor
MetadataLookup::MetadataLookup(
    MetadataType type,
    LookupType subtype,
    QVariant data,
    LookupStep step,
    bool automatic,
    bool handleimages,
    bool allowoverwrites,
    bool allowgeneric,
    bool preferdvdorder,
    const QString &host,
    const QString &filename,
    const QString &title,
    const QStringList &categories,
    const float userrating,
    const QString &subtitle,
    const QString &description,
    uint chanid,
    const QString &channum,
    const QString &chansign,
    const QString &channame,
    const QString &chanplaybackfilters,
    const QString &recgroup,
    const QString &playgroup,
    const QString &seriesid,
    const QString &programid,
    const QString &storagegroup,
    const QDateTime startts,
    const QDateTime endts,
    const QDateTime recstartts,
    const QDateTime recendts,
    uint programflags,
    uint audioproperties,
    uint videoproperties,
    uint subtitletype,
    const uint year,
    const QDate releasedate,
    const QDateTime lastupdated,
    const uint runtime,
    const uint runtimesecs) :

    m_type(type),
    m_subtype(subtype),
    m_data(data),
    m_step(step),
    m_automatic(automatic),
    m_handleimages(handleimages),
    m_allowoverwrites(allowoverwrites),
    m_allowgeneric(allowgeneric),
    m_dvdorder(preferdvdorder),
    m_host(host),
    m_filename(filename),
    m_title(title),
    m_categories(categories),
    m_userrating(userrating),
    m_ratingcount(0),
    m_subtitle(subtitle),
    m_description(description),
    m_season(0),
    m_episode(0),
    m_chanid(chanid),
    m_channum(channum),
    m_chansign(chansign),
    m_channame(channame),
    m_chanplaybackfilters(chanplaybackfilters),
    m_recgroup(recgroup),
    m_playgroup(playgroup),
    m_seriesid(seriesid),
    m_programid(programid),
    m_storagegroup(storagegroup),
    m_startts(startts),
    m_endts(endts),
    m_recstartts(recstartts),
    m_recendts(recendts),
    m_programflags(programflags),
    m_audioproperties(audioproperties),
    m_videoproperties(videoproperties),
    m_subtitletype(subtitletype),
    m_year(year),
    m_releasedate(releasedate),
    m_lastupdated(lastupdated),
    m_runtime(runtime),
    m_runtimesecs(runtimesecs)
{
    m_tracknum = 0;
    m_popularity = 0;
    m_budget = 0;
    m_revenue = 0;
}

// XBMC NFO-style constructor
MetadataLookup::MetadataLookup(
    MetadataType type,
    LookupType subtype,
    QVariant data,
    LookupStep step,
    bool automatic,
    bool handleimages,
    bool allowoverwrites,
    bool allowgeneric,
    bool preferdvdorder,
    const QString &host,
    const QString &filename,
    const QString &title,
    const QStringList &categories,
    const float userrating,
    const QString &subtitle,
    const QString &tagline,
    const QString &description,
    uint season,
    uint episode,
    const QString &certification,
    const uint year,
    const QDate releasedate,
    const uint runtime,
    const uint runtimesecs,
    const QString &inetref,
    const PeopleMap people,
    const QString &trailerURL,
    const ArtworkMap artwork,
    DownloadMap downloads) :

    m_type(type),
    m_subtype(subtype),
    m_data(data),
    m_step(step),
    m_automatic(automatic),
    m_handleimages(handleimages),
    m_allowoverwrites(allowoverwrites),
    m_allowgeneric(allowgeneric),
    m_dvdorder(preferdvdorder),
    m_host(host),
    m_filename(filename),
    m_title(title),
    m_categories(categories),
    m_userrating(userrating),
    m_ratingcount(0),
    m_subtitle(subtitle),
    m_tagline(tagline),
    m_description(description),
    m_season(season),
    m_episode(episode),
    m_chanid(0),
    m_programflags(0),
    m_audioproperties(0),
    m_videoproperties(0),
    m_subtitletype(0),
    m_certification(certification),
    m_popularity(0),
    m_budget(0),
    m_revenue(0),
    m_tracknum(0),
    m_year(year),
    m_releasedate(releasedate),
    m_runtime(runtime),
    m_runtimesecs(runtimesecs),
    m_inetref(inetref),
    m_people(people),
    m_trailerURL(trailerURL),
    m_artwork(artwork),
    m_downloads(downloads)
{
}

MetadataLookup::~MetadataLookup()
{
}

QList<PersonInfo> MetadataLookup::GetPeople(PeopleType type) const
{
    QList<PersonInfo> ret;
    ret = m_people.values(type);
    return ret;
}

ArtworkList MetadataLookup::GetArtwork(VideoArtworkType type) const
{
    ArtworkList ret;
    ret = m_artwork.values(type);
    return ret;
}

void MetadataLookup::toMap(InfoMap &metadataMap)
{
    metadataMap["filename"] = m_filename;
    metadataMap["title"] = m_title;
    metadataMap["network"] = m_network;
    metadataMap["status"] = m_status;
    metadataMap["category"] = m_categories.join(", ");
    metadataMap["userrating"] = QString::number(m_userrating);
    metadataMap["ratingcount"] = QString::number(m_ratingcount);
    metadataMap["language"] = m_language;
    metadataMap["subtitle"] = m_subtitle;
    metadataMap["tagline"] = m_tagline;
    metadataMap["description"] = m_description;
    metadataMap["season"] = QString::number(m_season);
    metadataMap["episode"] = QString::number(m_episode);
    metadataMap["chanid"] = QString::number(m_chanid);
    metadataMap["channum"] = m_channum;
    metadataMap["callsign"] = m_chansign;
    metadataMap["channame"] = m_channame;
    metadataMap["playbackfilters"] = m_chanplaybackfilters;
    metadataMap["recgroup"] = m_recgroup;
    metadataMap["playgroup"] = m_playgroup;
    metadataMap["seriesid"] = m_seriesid;
    metadataMap["programid"] = m_programid;
    metadataMap["storagegroup"] = m_storagegroup;
    metadataMap["startts"] = MythDate::toString(m_startts, MythDate::kDateFull);
    metadataMap["endts"] = MythDate::toString(m_endts, MythDate::kDateFull);
    metadataMap["recstartts"] = MythDate::toString(m_recstartts, MythDate::kDateFull);
    metadataMap["recendts"] = MythDate::toString(m_recendts, MythDate::kDateFull);
    metadataMap["certification"] = m_certification;
    metadataMap["countries"] = m_countries.join(", ");
    metadataMap["popularity"] = QString::number(m_popularity);
    metadataMap["budget"] = QString::number(m_budget);
    metadataMap["revenue"] = QString::number(m_revenue);
    metadataMap["album"] = m_album;
    metadataMap["tracknum"] = QString::number(m_tracknum);
    metadataMap["system"] = m_system;
    metadataMap["year"] = QString::number(m_year);

    metadataMap["releasedate"] = MythDate::toString(
        m_releasedate, MythDate::kDateFull);
    metadataMap["lastupdated"] = MythDate::toString(m_lastupdated, MythDate::kDateFull);

    metadataMap["runtime"] = QCoreApplication::translate("(Common)",
                                                         "%n minute(s)",
                                                         "",
                                                         QCoreApplication::UnicodeUTF8,
                                                         m_runtime);

    metadataMap["runtimesecs"] = QCoreApplication::translate("(Common)",
                                                             "%n second(s)",
                                                             "",
                                                             QCoreApplication::UnicodeUTF8,
                                                             m_runtimesecs);
    metadataMap["inetref"] = m_inetref;
    metadataMap["collectionref"] = m_collectionref;
    metadataMap["tmsref"] = m_tmsref;
    metadataMap["imdb"] = m_imdb;
    metadataMap["studios"] = m_studios.join(", ");
    metadataMap["homepage"] = m_homepage;
    metadataMap["trailer"] = m_trailerURL;
}

MetadataLookup* LookupFromProgramInfo(ProgramInfo *pginfo)
{
    uint runtimesecs = pginfo->GetRecordingStartTime()
                           .secsTo(pginfo->GetRecordingEndTime());
    uint runtime = (runtimesecs/60);

    MetadataLookup *ret = new MetadataLookup(kMetadataRecording, kUnknownVideo,
        qVariantFromValue(pginfo), kLookupData, false, false, false, false, false,
        pginfo->GetHostname(),pginfo->GetBasename(),pginfo->GetTitle(),
        QStringList() << pginfo->GetCategory(), pginfo->GetStars() * 10,
        pginfo->GetSubtitle(), pginfo->GetDescription(), pginfo->GetChanID(),
        pginfo->GetChanNum(), pginfo->GetChannelSchedulingID(),
        pginfo->GetChannelName(), pginfo->GetChannelPlaybackFilters(),
        pginfo->GetRecordingGroup(), pginfo->GetPlaybackGroup(),
        pginfo->GetSeriesID(), pginfo->GetProgramID(), pginfo->GetStorageGroup(),
        pginfo->GetScheduledStartTime(), pginfo->GetScheduledEndTime(),
        pginfo->GetRecordingStartTime(), pginfo->GetRecordingEndTime(),
        pginfo->GetProgramFlags(), pginfo->GetAudioProperties(),
        pginfo->GetVideoProperties(), pginfo->GetSubtitleType(),
        pginfo->GetYearOfInitialRelease(), pginfo->GetOriginalAirDate(),
        pginfo->GetLastModifiedTime(), runtime, runtimesecs);

    ret->SetSeason(pginfo->GetSeason());
    ret->SetEpisode(pginfo->GetEpisode());
    ret->SetInetref(pginfo->GetInetRef());

    return ret;
}

QDomDocument CreateMetadataXML(MetadataLookupList list)
{
    QDomDocument doc("MythMetadataXML");

    QDomElement root = doc.createElement("metadata");
    doc.appendChild(root);

    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
    {
        CreateMetadataXMLItem(*i, root, doc);
    }

    return doc;
}

QDomDocument CreateMetadataXML(MetadataLookup *lookup)
{
    QDomDocument doc("MythMetadataXML");

    QDomElement root = doc.createElement("metadata");
    doc.appendChild(root);

    CreateMetadataXMLItem(lookup, root, doc);

    return doc;
}

QDomDocument CreateMetadataXML(ProgramInfo *pginfo)
{
    QDomDocument doc("MythMetadataXML");

    MetadataLookup *lookup = LookupFromProgramInfo(pginfo);
    if (lookup)
        doc = CreateMetadataXML(lookup);

    delete lookup;
    lookup = NULL;

    return doc;
}

void CreateMetadataXMLItem(MetadataLookup *lookup,
                           QDomElement placetoadd,
                           QDomDocument docroot)
{
    if (!lookup)
        return;

    QDomElement item = docroot.createElement("item");
    placetoadd.appendChild(item);
    QString RFC822("ddd, d MMMM yyyy hh:mm:ss");

    // Language
    if (!lookup->GetLanguage().isEmpty())
    {
        QDomElement language = docroot.createElement("language");
        item.appendChild(language);
        language.appendChild(docroot.createTextNode(lookup->GetLanguage()));
    }
    // Title
    if (!lookup->GetTitle().isEmpty())
    {
        QDomElement title = docroot.createElement("title");
        item.appendChild(title);
        title.appendChild(docroot.createTextNode(lookup->GetTitle()));
    }
    // Subtitle
    if (!lookup->GetSubtitle().isEmpty())
    {
        QDomElement subtitle = docroot.createElement("subtitle");
        item.appendChild(subtitle);
        subtitle.appendChild(docroot.createTextNode(lookup->GetSubtitle()));
    }
    // Network
    if (!lookup->GetNetwork().isEmpty())
    {
        QDomElement network = docroot.createElement("network");
        item.appendChild(network);
        network.appendChild(docroot.createTextNode(lookup->GetNetwork()));
    }
    // Status
    if (!lookup->GetStatus().isEmpty())
    {
        QDomElement status = docroot.createElement("status");
        item.appendChild(status);
        status.appendChild(docroot.createTextNode(lookup->GetStatus()));
    }
    // Season
    if (lookup->GetSeason() > 0 || lookup->GetEpisode() > 0)
    {
        QDomElement season = docroot.createElement("season");
        item.appendChild(season);
        season.appendChild(docroot.createTextNode(
                           QString::number(lookup->GetSeason())));
    }
    // Episode
    if (lookup->GetSeason() > 0 || lookup->GetEpisode() > 0)
    {
        QDomElement episode = docroot.createElement("episode");
        item.appendChild(episode);
        episode.appendChild(docroot.createTextNode(
                           QString::number(lookup->GetEpisode())));
    }
    // Tagline
    if (!lookup->GetTagline().isEmpty())
    {
        QDomElement tagline = docroot.createElement("tagline");
        item.appendChild(tagline);
        tagline.appendChild(docroot.createTextNode(lookup->GetTagline()));
    }
    // Plot
    if (!lookup->GetDescription().isEmpty())
    {
        QDomElement desc = docroot.createElement("description");
        item.appendChild(desc);
        desc.appendChild(docroot.createTextNode(lookup->GetDescription()));
    }
    // Album Name
    if (!lookup->GetAlbumTitle().isEmpty())
    {
        QDomElement albumname = docroot.createElement("albumname");
        item.appendChild(albumname);
        albumname.appendChild(docroot.createTextNode(lookup->GetAlbumTitle()));
    }
    // Inetref
    if (!lookup->GetInetref().isEmpty())
    {
        QDomElement inetref = docroot.createElement("inetref");
        item.appendChild(inetref);
        inetref.appendChild(docroot.createTextNode(lookup->GetInetref()));
    }
    // Collectionref
    if (!lookup->GetCollectionref().isEmpty())
    {
        QDomElement collectionref = docroot.createElement("collectionref");
        item.appendChild(collectionref);
        collectionref.appendChild(docroot.createTextNode(lookup->GetCollectionref()));
    }
    // TMSref/SeriesID
    if (!lookup->GetTMSref().isEmpty())
    {
        QDomElement tmsref = docroot.createElement("tmsref");
        item.appendChild(tmsref);
        tmsref.appendChild(docroot.createTextNode(lookup->GetTMSref()));
    }
    // IMDB
    if (!lookup->GetIMDB().isEmpty())
    {
        QDomElement imdb = docroot.createElement("imdb");
        item.appendChild(imdb);
        imdb.appendChild(docroot.createTextNode(lookup->GetIMDB()));
    }
    // Home Page
    if (!lookup->GetHomepage().isEmpty())
    {
        QDomElement homepage = docroot.createElement("homepage");
        item.appendChild(homepage);
        homepage.appendChild(docroot.createTextNode(lookup->GetHomepage()));
    }
    // Trailer
    if (!lookup->GetTrailerURL().isEmpty())
    {
        QDomElement trailer = docroot.createElement("trailer");
        item.appendChild(trailer);
        trailer.appendChild(docroot.createTextNode(lookup->GetTrailerURL()));
    }
    // Chan ID
    if (lookup->GetChanId() > 0)
    {
        QDomElement chanid = docroot.createElement("chanid");
        item.appendChild(chanid);
        chanid.appendChild(docroot.createTextNode(
                           QString::number(lookup->GetChanId())));
    }
    // Channel Number
    if (!lookup->GetChanNum().isEmpty())
    {
        QDomElement channum = docroot.createElement("channum");
        item.appendChild(channum);
        channum.appendChild(docroot.createTextNode(lookup->GetChanNum()));
    }
    // Callsign
    if (!lookup->GetChanSign().isEmpty())
    {
        QDomElement callsign = docroot.createElement("chansign");
        item.appendChild(callsign);
        callsign.appendChild(docroot.createTextNode(lookup->GetChanSign()));
    }
    // Channel Name
    if (!lookup->GetChanName().isEmpty())
    {
        QDomElement channame = docroot.createElement("channame");
        item.appendChild(channame);
        channame.appendChild(docroot.createTextNode(lookup->GetChanName()));
    }
    // Playback Filters
    if (!lookup->GetChanPlaybackFilters().isEmpty())
    {
        QDomElement filters = docroot.createElement("filters");
        item.appendChild(filters);
        filters.appendChild(docroot.createTextNode(
                            lookup->GetChanPlaybackFilters()));
    }
    // Recording Group
    if (!lookup->GetRecGroup().isEmpty())
    {
        QDomElement recgroup = docroot.createElement("recgroup");
        item.appendChild(recgroup);
        recgroup.appendChild(docroot.createTextNode(lookup->GetRecGroup()));
    }
    // Playback Group
    if (!lookup->GetPlayGroup().isEmpty())
    {
        QDomElement playgroup = docroot.createElement("playgroup");
        item.appendChild(playgroup);
        playgroup.appendChild(docroot.createTextNode(lookup->GetPlayGroup()));
    }
    // SeriesID
    if (!lookup->GetSeriesId().isEmpty())
    {
        QDomElement seriesid = docroot.createElement("seriesid");
        item.appendChild(seriesid);
        seriesid.appendChild(docroot.createTextNode(lookup->GetSeriesId()));
    }
    // ProgramID
    if (!lookup->GetProgramId().isEmpty())
    {
        QDomElement programid = docroot.createElement("programid");
        item.appendChild(programid);
        programid.appendChild(docroot.createTextNode(lookup->GetProgramId()));
    }
    // Storage Group
    if (!lookup->GetStorageGroup().isEmpty())
    {
        QDomElement sgroup = docroot.createElement("storagegroup");
        item.appendChild(sgroup);
        sgroup.appendChild(docroot.createTextNode(lookup->GetStorageGroup()));
    }
    // Start TS
    if (!lookup->GetStartTS().isNull())
    {
        QDomElement startts = docroot.createElement("startts");
        item.appendChild(startts);
        startts.appendChild(docroot.createTextNode(
                                lookup->GetStartTS().toString(RFC822)));
    }
    // End TS
    if (!lookup->GetEndTS().isNull())
    {
        QDomElement endts = docroot.createElement("endts");
        item.appendChild(endts);
        endts.appendChild(docroot.createTextNode(
                          lookup->GetEndTS().toString(RFC822)));
    }
    // Rec Start TS
    if (!lookup->GetRecStartTS().isNull())
    {
        QDomElement recstartts = docroot.createElement("recstartts");
        item.appendChild(recstartts);
        recstartts.appendChild(docroot.createTextNode(
                          lookup->GetRecStartTS().toString(RFC822)));
    }
    // Rec End TS
    if (!lookup->GetRecEndTS().isNull())
    {
        QDomElement recendts = docroot.createElement("recendts");
        item.appendChild(recendts);
        recendts.appendChild(docroot.createTextNode(
                          lookup->GetRecEndTS().toString(RFC822)));
    }
    // Program Flags
    if (lookup->GetProgramFlags() > 0)
    {
        QDomElement progflags = docroot.createElement("programflags");
        item.appendChild(progflags);
        progflags.appendChild(docroot.createTextNode(
                          QString::number(lookup->GetProgramFlags())));
    }
    // Audio Properties
    if (lookup->GetAudioProperties() > 0)
    {
        QDomElement audioprops = docroot.createElement("audioproperties");
        item.appendChild(audioprops);
        audioprops.appendChild(docroot.createTextNode(
                          QString::number(lookup->GetAudioProperties())));
    }
    // Video Properties
    if (lookup->GetVideoProperties() > 0)
    {
        QDomElement videoprops = docroot.createElement("videoproperties");
        item.appendChild(videoprops);
        videoprops.appendChild(docroot.createTextNode(
                          QString::number(lookup->GetVideoProperties())));
    }
    // Subtitle Type
    if (lookup->GetSubtitleType() > 0)
    {
        QDomElement subprops = docroot.createElement("subtitletype");
        item.appendChild(subprops);
        subprops.appendChild(docroot.createTextNode(
                          QString::number(lookup->GetSubtitleType())));
    }
    // Release Date
    if (!lookup->GetReleaseDate().isNull())
    {
        QDomElement releasedate = docroot.createElement("releasedate");
        item.appendChild(releasedate);
        releasedate.appendChild(docroot.createTextNode(
                      lookup->GetReleaseDate().toString("yyyy-MM-dd")));
    }
    // Last Updated
    if (!lookup->GetLastUpdated().isNull())
    {
        QDomElement lastupdated = docroot.createElement("lastupdated");
        item.appendChild(lastupdated);
        lastupdated.appendChild(docroot.createTextNode(
                      lookup->GetLastUpdated().toString(RFC822)));
    }
    // User Rating
    if (lookup->GetUserRating() > 0)
    {
        QDomElement userrating = docroot.createElement("userrating");
        item.appendChild(userrating);
        userrating.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetUserRating())));
    }
    // Rating Count
    if (lookup->GetRatingCount() > 0)
    {
        QDomElement ratingcount = docroot.createElement("ratingcount");
        item.appendChild(ratingcount);
        ratingcount.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetRatingCount())));
    }
    // Track Number
    if (lookup->GetTrackNumber() > 0)
    {
        QDomElement tracknum = docroot.createElement("tracknum");
        item.appendChild(tracknum);
        tracknum.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetTrackNumber())));
    }
    // Popularity
    if (lookup->GetPopularity() > 0)
    {
        QDomElement popularity = docroot.createElement("popularity");
        item.appendChild(popularity);
        popularity.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetPopularity())));
    }
    // Budget
    if (lookup->GetBudget() > 0)
    {
        QDomElement budget = docroot.createElement("budget");
        item.appendChild(budget);
        budget.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetBudget())));
    }
    // Revenue
    if (lookup->GetRevenue() > 0)
    {
        QDomElement revenue = docroot.createElement("revenue");
        item.appendChild(revenue);
        revenue.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetRevenue())));
    }
    // Runtime
    if (lookup->GetRuntime() > 0)
    {
        QDomElement runtime = docroot.createElement("runtime");
        item.appendChild(runtime);
        runtime.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetRuntime())));
    }
    // Runtimesecs
    if (lookup->GetRuntimeSeconds() > 0)
    {
        QDomElement runtimesecs = docroot.createElement("runtimesecs");
        item.appendChild(runtimesecs);
        runtimesecs.appendChild(docroot.createTextNode(QString::number(
                       lookup->GetRuntimeSeconds())));
    }

    if (lookup->GetCertification().size())
        AddCertifications(lookup, item, docroot);
    if (lookup->GetCategories().size())
        AddCategories(lookup, item, docroot);
    if (lookup->GetStudios().size())
        AddStudios(lookup, item, docroot);
    if (lookup->GetCountries().size())
        AddCountries(lookup, item, docroot);
}

void AddCertifications(MetadataLookup *lookup,
                       QDomElement placetoadd,
                       QDomDocument docroot)
{
    QString certstr = lookup->GetCertification();
    QDomElement certifications = docroot.createElement("certifications");
    placetoadd.appendChild(certifications);

    QDomElement cert = docroot.createElement("certification");
    certifications.appendChild(cert);
    cert.setAttribute("locale", gCoreContext->GetLocale()->GetCountryCode());
    cert.setAttribute("name", certstr);
}

void AddCategories(MetadataLookup *lookup,
                   QDomElement placetoadd,
                   QDomDocument docroot)
{
    QStringList cats = lookup->GetCategories();
    QDomElement categories = docroot.createElement("categories");
    placetoadd.appendChild(categories);

    for (QStringList::const_iterator i = cats.begin();
            i != cats.end(); ++i)
    {
        QDomElement cat = docroot.createElement("category");
        categories.appendChild(cat);
        cat.setAttribute("type", "genre");
        cat.setAttribute("name", *i);
    }
}

void AddStudios(MetadataLookup *lookup,
                   QDomElement placetoadd,
                   QDomDocument docroot)
{
    QStringList studs = lookup->GetStudios();
    QDomElement studios = docroot.createElement("studios");
    placetoadd.appendChild(studios);

    for (QStringList::const_iterator i = studs.begin();
            i != studs.end(); ++i)
    {
        QDomElement stud = docroot.createElement("studio");
        studios.appendChild(stud);
        stud.setAttribute("name", *i);
    }
}

void AddCountries(MetadataLookup *lookup,
                  QDomElement placetoadd,
                  QDomDocument docroot)
{
    QStringList counts = lookup->GetCountries();
    QDomElement countries = docroot.createElement("countries");
    placetoadd.appendChild(countries);

    for (QStringList::const_iterator i = counts.begin();
            i != counts.end(); ++i)
    {
        QDomElement count = docroot.createElement("country");
        countries.appendChild(count);
        count.setAttribute("name", *i);
    }
}

MetadataLookup* ParseMetadataItem(const QDomElement& item,
                                  MetadataLookup *lookup,
                                  bool passseas)
{
    if (!lookup)
        return new MetadataLookup();

    uint season = 0, episode = 0, chanid = 0, programflags = 0,
         audioproperties = 0, videoproperties = 0, subtitletype = 0,
         tracknum = 0, popularity = 0, budget = 0, revenue = 0,
         year = 0, runtime = 0, runtimesecs = 0, ratingcount = 0;
    QString title, network, status, subtitle, tagline, description, certification,
        channum, chansign, channame, chanplaybackfilters, recgroup,
        playgroup, seriesid, programid, storagegroup, album, system,
        inetref, collectionref, tmsref, imdb, homepage, trailerURL, language;
    QStringList categories, countries, studios;
    float userrating = 0;
    QDate releasedate;
    QDateTime lastupdated, startts, endts, recstartts, recendts;
    PeopleMap people;
    ArtworkMap artwork;

    // Get the easy parses
    language = item.firstChildElement("language").text();
    title = Parse::UnescapeHTML(item.firstChildElement("title").text());
    network = Parse::UnescapeHTML(item.firstChildElement("network").text());
    status = Parse::UnescapeHTML(item.firstChildElement("status").text());
    subtitle = Parse::UnescapeHTML(item.firstChildElement("subtitle").text());
    tagline = Parse::UnescapeHTML(item.firstChildElement("tagline").text());
    description = Parse::UnescapeHTML(item.firstChildElement("description").text());
    album = Parse::UnescapeHTML(item.firstChildElement("albumname").text());
    inetref = item.firstChildElement("inetref").text();
    collectionref = item.firstChildElement("collectionref").text();
    tmsref = item.firstChildElement("tmsref").text();
    imdb = item.firstChildElement("imdb").text();
    homepage = item.firstChildElement("homepage").text();
    trailerURL = item.firstChildElement("trailer").text();

    // ProgramInfo specific parsing
    chanid = item.firstChildElement("chanid").text().toUInt();
    channum = item.firstChildElement("channum").text();
    chansign = item.firstChildElement("chansign").text();
    channame = item.firstChildElement("channame").text();
    chanplaybackfilters = item.firstChildElement("chanplaybackfilters").text();
    recgroup = item.firstChildElement("recgroup").text();
    playgroup = item.firstChildElement("playgroup").text();
    seriesid = item.firstChildElement("seriesid").text();
    programid = item.firstChildElement("programid").text();
    storagegroup = item.firstChildElement("storagegroup").text();
    startts = RFC822TimeToQDateTime(item.
                      firstChildElement("startts").text());
    endts = RFC822TimeToQDateTime(item.
                      firstChildElement("endts").text());
    recstartts = RFC822TimeToQDateTime(item.
                      firstChildElement("recstartts").text());
    recendts = RFC822TimeToQDateTime(item.
                      firstChildElement("recendts").text());
    programflags = item.firstChildElement("programflags").text().toUInt();
    audioproperties = item.firstChildElement("audioproperties").text().toUInt();
    videoproperties = item.firstChildElement("videoproperties").text().toUInt();
    subtitletype = item.firstChildElement("subtitletype").text().toUInt();

    QString tmpDate = item.firstChildElement("releasedate").text();
    if (!tmpDate.isEmpty())
        releasedate = QDate::fromString(tmpDate, "yyyy-MM-dd");
    lastupdated = RFC822TimeToQDateTime(item.
                      firstChildElement("lastupdated").text());

    userrating = item.firstChildElement("userrating").text().toFloat();
    ratingcount = item.firstChildElement("ratingcount").text().toUInt();
    tracknum = item.firstChildElement("tracknum").text().toUInt();
    popularity = item.firstChildElement("popularity").text().toUInt();
    budget = item.firstChildElement("budget").text().toUInt();
    revenue = item.firstChildElement("revenue").text().toUInt();
    year = item.firstChildElement("year").text().toUInt();
    if (!year && !releasedate.isNull())
        year = releasedate.toString("yyyy").toUInt();
    runtime = item.firstChildElement("runtime").text().toUInt();
    runtimesecs = item.firstChildElement("runtimesecs").text().toUInt();

    QDomElement systems = item.firstChildElement("systems");
    if (!systems.isNull())
    {
        QDomElement firstSystem = systems.firstChildElement("system");
        if (!firstSystem.isNull())
            system = firstSystem.text();
    }

    // TODO: Once TMDB supports certification per-locale, come back and match
    // locale of myth to certification locale.
    QDomElement certifications = item.firstChildElement("certifications");
    QList< QPair<QString,QString> > ratinglist;
    if (!certifications.isNull())
    {
        QDomElement cert = certifications.firstChildElement("certification");
        if (!cert.isNull())
        {
            while (!cert.isNull())
            {
                if (cert.hasAttribute("locale") && cert.hasAttribute("name"))
                {
                    QPair<QString,QString> newcert(cert.attribute("locale"),
                                             cert.attribute("name"));
                    ratinglist.append(newcert);
                }
                cert = cert.nextSiblingElement("certification");
            }
        }
    }
    // HACK: To go away when someone supports ratings by locale.
    if (!ratinglist.isEmpty())
        certification = ratinglist.takeFirst().second;

    // Categories
    QDomElement categoriesxml = item.firstChildElement("categories");
    if (!categoriesxml.isNull())
    {
        QDomElement cat = categoriesxml.firstChildElement("category");
        if (!cat.isNull())
        {
            while (!cat.isNull())
            {
                if (cat.hasAttribute("name"))
                    categories.append(cat.attribute("name"));
                cat = cat.nextSiblingElement("category");
            }
        }
    }

    // Countries
    QDomElement countriesxml = item.firstChildElement("countries");
    if (!countriesxml.isNull())
    {
        QDomElement cntry = countriesxml.firstChildElement("country");
        if (!cntry.isNull())
        {
            while (!cntry.isNull())
            {
                if (cntry.hasAttribute("name"))
                    countries.append(cntry.attribute("name"));
                cntry = cntry.nextSiblingElement("country");
            }
        }
    }

    // Studios
    QDomElement studiosxml = item.firstChildElement("studios");
    if (!studiosxml.isNull())
    {
        QDomElement studio = studiosxml.firstChildElement("studio");
        if (!studio.isNull())
        {
            while (!studio.isNull())
            {
                if (studio.hasAttribute("name"))
                    studios.append(studio.attribute("name"));
                studio = studio.nextSiblingElement("studio");
            }
        }
    }

    // People
    QDomElement peoplexml = item.firstChildElement("people");
    if (!peoplexml.isNull())
    {
        people = ParsePeople(peoplexml);
    }

    // Artwork
    QDomElement artworkxml = item.firstChildElement("images");
    if (!artworkxml.isNull())
    {
        artwork = ParseArtwork(artworkxml);
    }

    // Have to handle season and episode a little differently.
    // If the query object comes in with a season or episode number,
    // we want to pass that through.  However, if we are doing a title/subtitle
    // lookup, we need to parse for season and episode.
    if (passseas)
    {
        season = lookup->GetSeason();
        episode = lookup->GetEpisode();
    }
    else
    {
        if (lookup->GetPreferDVDOrdering())
        {
            season = item.firstChildElement("dvdseason").text().toUInt();
            episode = item.firstChildElement("dvdepisode").text().toUInt();
        }

        if ((season == 0) && (episode == 0))
        {
            season = item.firstChildElement("season").text().toUInt();
            episode = item.firstChildElement("episode").text().toUInt();
        }
        LOG(VB_GENERAL, LOG_INFO, QString("Result Found, Season %1 Episode %2")
            .arg(season).arg(episode));
    }

    return new MetadataLookup(lookup->GetType(), lookup->GetSubtype(),
        lookup->GetData(), lookup->GetStep(), lookup->GetAutomatic(),
        lookup->GetHandleImages(), lookup->GetAllowOverwrites(),
        lookup->GetAllowGeneric(), lookup->GetPreferDVDOrdering(), lookup->GetHost(),
        lookup->GetFilename(), title, network, status, categories, userrating,
        ratingcount, language, subtitle, tagline, description, season,
        episode, chanid, channum, chansign, channame,
        chanplaybackfilters, recgroup, playgroup, seriesid, programid,
        storagegroup, startts, endts, recstartts, recendts, programflags,
        audioproperties, videoproperties, subtitletype, certification,
        countries, popularity, budget, revenue, album, tracknum, system, year,
        releasedate, lastupdated, runtime, runtimesecs, inetref, collectionref,
        tmsref, imdb, people, studios, homepage, trailerURL, artwork, DownloadMap());
}

MetadataLookup* ParseMetadataMovieNFO(const QDomElement& item,
                                  MetadataLookup *lookup)
{
    if (!lookup)
        return new MetadataLookup();

    uint year = 0, runtime = 0, runtimesecs = 0,
         season = 0, episode = 0;
    QString title, subtitle, tagline, description,
        inetref, trailer, certification;
    float userrating = 0;
    QDate releasedate;
    QStringList categories;
    PeopleMap people;
    ArtworkMap artwork;

    // Get the easy parses
    QString titletmp;
    if (item.tagName() == "movie")
        title = Parse::UnescapeHTML(item.firstChildElement("title").text());
    else if (item.tagName() == "episodedetails")
        subtitle = Parse::UnescapeHTML(item.firstChildElement("title").text());
    userrating = item.firstChildElement("rating").text().toFloat();
    year = item.firstChildElement("year").text().toUInt();
    season = item.firstChildElement("season").text().toUInt();
    episode = item.firstChildElement("episode").text().toUInt();
    description = Parse::UnescapeHTML(item.firstChildElement("plot").text());
    tagline = Parse::UnescapeHTML(item.firstChildElement("tagline").text());
    inetref = item.firstChildElement("id").text();
    trailer = item.firstChildElement("trailer").text();
    certification = item.firstChildElement("mpaa").text();
    categories.append(item.firstChildElement("genre").text());

    QString tmpDate = item.firstChildElement("releasedate").text();
    if (!tmpDate.isEmpty())
        releasedate = QDate::fromString(tmpDate, "yyyy-MM-dd");
    else if (year > 0)
        releasedate = QDate::fromString(QString::number(year), "yyyy");

    runtime = item.firstChildElement("runtime").text()
                                               .remove(QRegExp("[A-Za-z]"))
                                               .trimmed().toUInt();
    runtimesecs = runtime * 60;

    QDomElement actor = item.firstChildElement("actor");
    if (!actor.isNull())
    {
        while (!actor.isNull())
        {
            PersonInfo info;
            info.name = actor.firstChildElement("name").text();
            info.role = actor.firstChildElement("role").text();
            info.thumbnail = actor.firstChildElement("thumb").text();
            people.insert(kPersonActor, info);
            actor = actor.nextSiblingElement("actor");
        }
    }

    QString director = item.firstChildElement("director").text();
    if (!director.isEmpty())
    {
        PersonInfo info;
        info.name = director;
        people.insert(kPersonDirector, info);
    }

    return new MetadataLookup(lookup->GetType(), lookup->GetSubtype(),
        lookup->GetData(), lookup->GetStep(),
        lookup->GetAutomatic(), lookup->GetHandleImages(),
        lookup->GetAllowOverwrites(), lookup->GetAllowGeneric(),
        lookup->GetPreferDVDOrdering(), lookup->GetHost(),
        lookup->GetFilename(), title, categories,
        userrating, subtitle, tagline, description, season, episode,
        certification, year, releasedate, runtime, runtimesecs,
        inetref, people, trailer, artwork, DownloadMap());
}

PeopleMap ParsePeople(QDomElement people)
{
    PeopleMap ret;

    QDomElement person = people.firstChildElement("person");
    if (!person.isNull())
    {
        while (!person.isNull())
        {
            if (person.hasAttribute("job"))
            {
                QString jobstring = person.attribute("job");
                PeopleType type;
                if (jobstring.toLower() == "actor")
                    type = kPersonActor;
                else if (jobstring.toLower() == "author")
                    type = kPersonAuthor;
                else if (jobstring.toLower() == "producer")
                    type = kPersonProducer;
                else if (jobstring.toLower() == "executive producer")
                    type = kPersonExecProducer;
                else if (jobstring.toLower() == "director")
                    type = kPersonDirector;
                else if (jobstring.toLower() == "cinematographer")
                    type = kPersonCinematographer;
                else if (jobstring.toLower() == "composer")
                    type = kPersonComposer;
                else if (jobstring.toLower() == "editor")
                    type = kPersonEditor;
                else if (jobstring.toLower() == "casting")
                    type = kPersonCastingDirector;
                else if (jobstring.toLower() == "artist")
                    type = kPersonArtist;
                else if (jobstring.toLower() == "album artist")
                    type = kPersonAlbumArtist;
                else if (jobstring.toLower() == "guest star")
                    type = kPersonGuestStar;
                else
                    type = kPersonActor;

                PersonInfo info;
                if (person.hasAttribute("name"))
                    info.name = person.attribute("name");
                if (person.hasAttribute("character"))
                    info.role = person.attribute("character");
                if (person.hasAttribute("thumb"))
                    info.thumbnail = person.attribute("thumb");
                if (person.hasAttribute("url"))
                    info.url = person.attribute("url");

                ret.insert(type,info);
            }
            person = person.nextSiblingElement("person");
        }
    }
    return ret;
}

ArtworkMap ParseArtwork(QDomElement artwork)
{
    ArtworkMap ret;

    QDomElement image = artwork.firstChildElement("image");
    if (!image.isNull())
    {
        while (!image.isNull())
        {
            if (image.hasAttribute("type"))
            {
                QString typestring = image.attribute("type");
                VideoArtworkType type;
                if (typestring.toLower() == "coverart")
                    type = kArtworkCoverart;
                else if (typestring.toLower() == "fanart")
                    type = kArtworkFanart;
                else if (typestring.toLower() == "banner")
                    type = kArtworkBanner;
                else if (typestring.toLower() == "screenshot")
                    type = kArtworkScreenshot;
                else if (typestring.toLower() == "poster")
                    type = kArtworkPoster;
                else if (typestring.toLower() == "back cover")
                    type = kArtworkBackCover;
                else if (typestring.toLower() == "inside cover")
                    type = kArtworkInsideCover;
                else if (typestring.toLower() == "cd image")
                    type = kArtworkCDImage;
                else
                    type = kArtworkCoverart;

                ArtworkInfo info;
                if (image.hasAttribute("thumb"))
                    info.thumbnail = image.attribute("thumb");
                if (image.hasAttribute("url"))
                    info.url = image.attribute("url");
                if (image.hasAttribute("width"))
                    info.width = image.attribute("width").toUInt();
                else
                    info.width = 0;
                if (image.hasAttribute("height"))
                    info.height = image.attribute("height").toUInt();
                else
                    info.height = 0;

                ret.insert(type,info);
            }
            image = image.nextSiblingElement("image");
        }
    }
    return ret;
}

int editDistance( const QString& s, const QString& t )
{
#define D( i, j ) d[(i) * n + (j)]
    int i;
    int j;
    int m = s.length() + 1;
    int n = t.length() + 1;
    int *d = new int[m * n];
    int result;

    for ( i = 0; i < m; i++ )
      D( i, 0 ) = i;
    for ( j = 0; j < n; j++ )
      D( 0, j ) = j;
    for ( i = 1; i < m; i++ )
    {
        for ( j = 1; j < n; j++ )
        {
            if ( s[i - 1] == t[j - 1] )
                D( i, j ) = D( i - 1, j - 1 );
            else
            {
                int x = D( i - 1, j );
                int y = D( i - 1, j - 1 );
                int z = D( i, j - 1 );
                D( i, j ) = 1 + qMin( qMin(x, y), z );
            }
        }
    }
    result = D( m - 1, n - 1 );
    delete[] d;
    return result;
#undef D
}

QString nearestName(const QString& actual, const QStringList& candidates)
{
    int deltaBest = 10000;
    int numBest = 0;
    int tolerance = gCoreContext->GetNumSetting("MetadataLookupTolerance", 5);
    QString best;

    QStringList::ConstIterator i = candidates.begin();
    while ( i != candidates.end() )
    {
        if ( (*i)[0] == actual[0] )
        {
            int delta = editDistance( actual, *i );
            if ( delta < deltaBest )
            {
                deltaBest = delta;
                numBest = 1;
                best = *i;
            }
            else if ( delta == deltaBest )
            {
                numBest++;
            }
        }
        ++i;
    }

    if ( numBest == 1 && deltaBest <= tolerance &&
       actual.length() + best.length() >= 5 )
        return best;
    else
        return QString();
}

QDateTime RFC822TimeToQDateTime(const QString& t)
{
    QMap<QString, int> TimezoneOffsets;

    if (t.size() < 20)
        return QDateTime();

    QString time = t.simplified();
    short int hoursShift = 0, minutesShift = 0;

    QStringList tmp = time.split(' ');
    if (tmp.isEmpty())
        return QDateTime();
    if (tmp. at(0).contains(QRegExp("\\D")))
        tmp.removeFirst();
    if (tmp.size() != 5)
        return QDateTime();
    QString timezone = tmp.takeAt(tmp.size() -1);
    if (timezone.size() == 5)
    {
        bool ok;
        int tz = timezone.toInt(&ok);
        if(ok)
        {
            hoursShift = tz / 100;
            minutesShift = tz % 100;
        }
    }
    else
        hoursShift = TimezoneOffsets.value(timezone, 0);

    if (tmp.at(0).size() == 1)
        tmp[0].prepend("0");
    tmp [1].truncate(3);

    time = tmp.join(" ");

    QDateTime result;
    if (tmp.at(2).size() == 4)
        result = QLocale::c().toDateTime(time, "dd MMM yyyy hh:mm:ss");
    else
        result = QLocale::c().toDateTime(time, "dd MMM yy hh:mm:ss");
    if (result.isNull() || !result.isValid())
        return QDateTime();
    result = result.addSecs(hoursShift * 3600 * (-1) + minutesShift *60 * (-1));
    result.setTimeSpec(Qt::UTC);
    return result;
}

MetaGrabberScript::MetaGrabberScript(
    const QString &name,
    const QString &author,
    const QString &thumbnail,
    const QString &command,
    const GrabberType type,
    const QString &typestring,
    const QString &description,
    const float version
    ) :
    m_name(name),
    m_author(author),
    m_thumbnail(thumbnail),
    m_command(command),
    m_type(type),
    m_typestring(typestring),
    m_description(description),
    m_version(version)
{
}

MetaGrabberScript::~MetaGrabberScript()
{
}

MetaGrabberScript* ParseGrabberVersion(const QDomElement& item)
{
    QString name, author, thumbnail, command, description, typestring;
    float version = 0;
    GrabberType type = kGrabberMovie;

    name = item.firstChildElement("name").text();
    author = item.firstChildElement("author").text();
    thumbnail = item.firstChildElement("thumbnail").text();
    command = item.firstChildElement("command").text();
    description = item.firstChildElement("description").text();
    version = item.firstChildElement("version").text().toFloat();
    typestring = item.firstChildElement("type").text();

    if (!typestring.isEmpty())
    {
        if (typestring.toLower() == "movie")
            type = kGrabberMovie;
        else if (typestring.toLower() == "television")
            type = kGrabberTelevision;
        else if (typestring.toLower() == "game")
            type = kGrabberGame;
        else if (typestring.toLower() == "music")
            type = kGrabberMusic;
    }

    return new MetaGrabberScript(name, author, thumbnail, command,
                             type, typestring, description, version);
}

void MetaGrabberScript::toMap(InfoMap &metadataMap)
{
    metadataMap["name"] = m_name;
    metadataMap["author"] = m_author;
    metadataMap["thumbnailfilename"] = m_thumbnail;
    metadataMap["command"] = m_command;
    metadataMap["description"] = m_description;
    metadataMap["version"] = QString::number(m_version);
    metadataMap["type"] = m_typestring;
}
