#include <QLocale>

#include "rssparse.h"
#include "mythcorecontext.h"
#include "metadatacommon.h"
#include "mythlogging.h"

// null constructor
MetadataLookup::MetadataLookup(void) :
    m_type(VID),
    m_data(),
    m_step(SEARCH),
    m_automatic(false),
    m_handleimages(false),
    m_allowoverwrites(false),
    m_dvdorder(false),
    m_host(),
    m_filename(),
    m_title(),
    m_categories(),
    m_userrating(0),
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
    QVariant data,
    LookupStep step,
    bool automatic,
    bool handleimages,
    bool allowoverwrites,
    bool preferdvdorder,
    QString host,
    QString filename,
    QString title,
    const QStringList categories,
    const float userrating,
    const QString language,
    QString subtitle,
    const QString tagline,
    const QString description,
    uint season,
    uint episode,
    uint chanid,
    const QString channum,
    const QString chansign,
    const QString channame,
    const QString chanplaybackfilters,
    const QString recgroup,
    const QString playgroup,
    const QString seriesid,
    const QString programid,
    const QString storagegroup,
    const QDateTime startts,
    const QDateTime endts,
    const QDateTime recstartts,
    const QDateTime recendts,
    uint programflags,
    uint audioproperties,
    uint videoproperties,
    uint subtitletype,
    const QString certification,
    const QStringList countries,
    const uint popularity,
    const uint budget,
    const uint revenue,
    QString album,
    uint tracknum,
    const QString system,
    const uint year,
    const QDate releasedate,
    const QDateTime lastupdated,
    const uint runtime,
    const uint runtimesecs,
    QString inetref,
    QString tmsref,
    QString imdb,
    const PeopleMap people,
    const QStringList studios,
    const QString homepage,
    const QString trailerURL,
    const ArtworkMap artwork,
    DownloadMap downloads) :

    m_type(type),
    m_data(data),
    m_step(step),
    m_automatic(automatic),
    m_handleimages(handleimages),
    m_allowoverwrites(allowoverwrites),
    m_dvdorder(preferdvdorder),
    m_host(host),
    m_filename(filename),
    m_title(title),
    m_categories(categories),
    m_userrating(userrating),
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
    QVariant data,
    LookupStep step,
    bool automatic,
    bool handleimages,
    bool allowoverwrites,
    bool preferdvdorder,
    QString host,
    QString filename,
    QString title,
    const QStringList categories,
    const float userrating,
    QString subtitle,
    const QString description,
    uint chanid,
    const QString channum,
    const QString chansign,
    const QString channame,
    const QString chanplaybackfilters,
    const QString recgroup,
    const QString playgroup,
    const QString seriesid,
    const QString programid,
    const QString storagegroup,
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
    m_data(data),
    m_step(step),
    m_automatic(automatic),
    m_handleimages(handleimages),
    m_allowoverwrites(allowoverwrites),
    m_dvdorder(preferdvdorder),
    m_host(host),
    m_filename(filename),
    m_title(title),
    m_categories(categories),
    m_userrating(userrating),
    m_subtitle(subtitle),
    m_description(description),
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
}

// XBMC NFO-style constructor
MetadataLookup::MetadataLookup(
    MetadataType type,
    QVariant data,
    LookupStep step,
    bool automatic,
    bool handleimages,
    bool allowoverwrites,
    bool preferdvdorder,
    QString host,
    QString filename,
    QString title,
    const QStringList categories,
    const float userrating,
    QString subtitle,
    const QString tagline,
    const QString description,
    uint season,
    uint episode,
    const QString certification,
    const uint year,
    const QDate releasedate,
    const uint runtime,
    const uint runtimesecs,
    QString inetref,
    const PeopleMap people,
    const QString trailerURL,
    const ArtworkMap artwork,
    DownloadMap downloads) :

    m_type(type),
    m_data(data),
    m_step(step),
    m_automatic(automatic),
    m_handleimages(handleimages),
    m_allowoverwrites(allowoverwrites),
    m_dvdorder(preferdvdorder),
    m_host(host),
    m_filename(filename),
    m_title(title),
    m_categories(categories),
    m_userrating(userrating),
    m_subtitle(subtitle),
    m_tagline(tagline),
    m_description(description),
    m_season(season),
    m_episode(episode),
    m_certification(certification),
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

void MetadataLookup::toMap(MetadataMap &metadataMap)
{
    QString dateformat = gCoreContext->GetSetting("DateFormat",
                                            "yyyy-MM-dd hh:mm");

    metadataMap["filename"] = m_filename;
    metadataMap["title"] = m_title;
    metadataMap["category"] = m_categories.join(", ");
    metadataMap["userrating"] = QString::number(m_userrating);
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
    metadataMap["startts"] = m_startts.toString(dateformat);
    metadataMap["endts"] = m_endts.toString(dateformat);
    metadataMap["recstartts"] = m_recstartts.toString(dateformat);
    metadataMap["recendts"] = m_recendts.toString(dateformat);
    metadataMap["certification"] = m_certification;
    metadataMap["countries"] = m_countries.join(", ");
    metadataMap["popularity"] = QString::number(m_popularity);
    metadataMap["budget"] = QString::number(m_budget);
    metadataMap["revenue"] = QString::number(m_revenue);
    metadataMap["album"] = m_album;
    metadataMap["tracknum"] = QString::number(m_tracknum);
    metadataMap["system"] = m_system;
    metadataMap["year"] = QString::number(m_year);

    metadataMap["releasedate"] = m_releasedate.toString(dateformat);
    metadataMap["lastupdated"] = m_lastupdated.toString(dateformat);

    metadataMap["runtime"] = QString::number(m_runtime) +
                                      QObject::tr(" Minutes");
    metadataMap["runtimesecs"] = QString::number(m_runtimesecs) +
                                          QObject::tr(" Seconds");
    metadataMap["inetref"] = m_inetref;
    metadataMap["tmsref"] = m_tmsref;
    metadataMap["imdb"] = m_imdb;
    metadataMap["studios"] = m_studios.join(", ");
    metadataMap["homepage"] = m_homepage;
    metadataMap["trailer"] = m_trailerURL;
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
         year = 0, runtime = 0, runtimesecs = 0;
    QString title, subtitle, tagline, description, certification,
        channum, chansign, channame, chanplaybackfilters, recgroup,
        playgroup, seriesid, programid, storagegroup, album, system,
        inetref, tmsref, imdb, homepage, trailerURL, language;
    QStringList categories, countries, studios;
    float userrating = 0;
    QDate releasedate;
    QDateTime lastupdated, startts, endts, recstartts, recendts;
    PeopleMap people;
    ArtworkMap artwork;

    // Get the easy parses
    title = Parse::UnescapeHTML(item.firstChildElement("title").text());
    subtitle = Parse::UnescapeHTML(item.firstChildElement("subtitle").text());
    tagline = Parse::UnescapeHTML(item.firstChildElement("tagline").text());
    description = Parse::UnescapeHTML(item.firstChildElement("description").text());
    album = Parse::UnescapeHTML(item.firstChildElement("albumname").text());
    inetref = item.firstChildElement("inetref").text();
    tmsref = item.firstChildElement("tmsref").text();
    imdb = item.firstChildElement("imdb").text();
    homepage = item.firstChildElement("homepage").text();
    trailerURL = item.firstChildElement("trailer").text();
    language = item.firstChildElement("language").text();

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
        VERBOSE(VB_GENERAL, QString("Parsing Season, %1 %2")
            .arg(season).arg(episode));
    }

    return new MetadataLookup(lookup->GetType(), lookup->GetData(),
        lookup->GetStep(), lookup->GetAutomatic(), lookup->GetHandleImages(),
        lookup->GetAllowOverwrites(), lookup->GetPreferDVDOrdering(),
        lookup->GetHost(), lookup->GetFilename(), title, categories,
        userrating, language, subtitle, tagline, description,
        season, episode, chanid, channum, chansign, channame,
        chanplaybackfilters, recgroup, playgroup, seriesid, programid,
        storagegroup, startts, endts, recstartts, recendts, programflags,
        audioproperties, videoproperties, subtitletype, certification,
        countries, popularity, budget, revenue, album, tracknum, system, year,
        releasedate, lastupdated, runtime, runtimesecs, inetref, tmsref,
        imdb, people, studios, homepage, trailerURL, artwork, DownloadMap());
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
            people.insert(ACTOR, info);
            actor = actor.nextSiblingElement("actor");
        }
    }

    QString director = item.firstChildElement("director").text();
    if (!director.isEmpty())
    {
        PersonInfo info;
        info.name = director;
        people.insert(DIRECTOR, info);
    }

    return new MetadataLookup(lookup->GetType(), lookup->GetData(),
        lookup->GetStep(), lookup->GetAutomatic(), lookup->GetHandleImages(),
        lookup->GetAllowOverwrites(), lookup->GetPreferDVDOrdering(),
        lookup->GetHost(), lookup->GetFilename(), title, categories,
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
                    type = ACTOR;
                else if (jobstring.toLower() == "author")
                    type = AUTHOR;
                else if (jobstring.toLower() == "producer")
                    type = PRODUCER;
                else if (jobstring.toLower() == "executive producer")
                    type = EXECPRODUCER;
                else if (jobstring.toLower() == "director")
                    type = DIRECTOR;
                else if (jobstring.toLower() == "cinematographer")
                    type = CINEMATOGRAPHER;
                else if (jobstring.toLower() == "composer")
                    type = COMPOSER;
                else if (jobstring.toLower() == "editor")
                    type = EDITOR;
                else if (jobstring.toLower() == "casting")
                    type = CASTINGDIRECTOR;
                else if (jobstring.toLower() == "artist")
                    type = ARTIST;
                else if (jobstring.toLower() == "album artist")
                    type = ALBUMARTIST;
                else if (jobstring.toLower() == "guest star")
                    type = GUESTSTAR;
                else
                    type = ACTOR;

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
                    type = COVERART;
                else if (typestring.toLower() == "fanart")
                    type = FANART;
                else if (typestring.toLower() == "banner")
                    type = BANNER;
                else if (typestring.toLower() == "screenshot")
                    type = SCREENSHOT;
                else if (typestring.toLower() == "poster")
                    type = POSTER;
                else if (typestring.toLower() == "back cover")
                    type = BACKCOVER;
                else if (typestring.toLower() == "inside cover")
                    type = INSIDECOVER;
                else if (typestring.toLower() == "cd image")
                    type = CDIMAGE;
                else
                    type = COVERART;

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
    return result.toLocalTime();
}

MetaGrabberScript::MetaGrabberScript(const QString name,
    const QString author,
    const QString thumbnail,
    const QString command,
    const GrabberType type,
    const QString typestring,
    const QString description,
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
    GrabberType type = GRAB_MOVIE;

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
            type = GRAB_MOVIE;
        else if (typestring.toLower() == "television")
            type = GRAB_TELEVISION;
        else if (typestring.toLower() == "game")
            type = GRAB_GAME;
        else if (typestring.toLower() == "music")
            type = GRAB_MUSIC;
    }

    return new MetaGrabberScript(name, author, thumbnail, command,
                             type, typestring, description, version);
}

void MetaGrabberScript::toMap(MetadataMap &metadataMap)
{
    metadataMap["name"] = m_name;
    metadataMap["author"] = m_author;
    metadataMap["thumbnailfilename"] = m_thumbnail;
    metadataMap["command"] = m_command;
    metadataMap["description"] = m_description;
    metadataMap["version"] = QString::number(m_version);
    metadataMap["type"] = m_typestring;
}
