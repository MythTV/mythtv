/*
 *  Class RecordingExtender
 *
 *  Copyright (c) David Hampton 2021
 *
 *   Based on the ideas in the standalone Myth Recording PHP code from
 *   Derek Battams <derek@battams.ca>.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// Qt
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

// MythTV
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"

// MythBackend
#include "recordingextender.h"
#include "scheduler.h"

#define LOC QString("RecExt: ")

/// Does the specified time fall within -3/+1 hour from now?
static constexpr int64_t kLookBackTime    { 3LL * 60 * 60 };
static constexpr int64_t kLookForwardTime { 1LL * 60 * 60 };

static constexpr std::chrono::minutes kExtensionTime {10};
static constexpr int kExtensionTimeInSec {
    (duration_cast<std::chrono::seconds>(kExtensionTime).count()) };
static const QRegularExpression kVersusPattern {R"(\s(at|@|vs\.?)\s)"};
static const QRegularExpression kSentencePattern {R"(:|\.+\s)"};

/// Does this recording status indicate that the recording is still ongoing.
///
/// @param[in] recstatus The status of a single program recording.
/// @returns true if the program is still being recorded. Returns
///          false if the recording has failed, stopped, etc.
static inline bool ValidRecordingStatus(RecStatus::Type recstatus)
{
    return (recstatus == RecStatus::Recording ||
            recstatus == RecStatus::Tuning ||
            recstatus == RecStatus::WillRecord ||
            recstatus == RecStatus::Pending);
}

/// Set the game scheduling information URL. The schedule pages
/// contain the team names, starting time, and game ID (which can be
/// use to create the game status link.)
///
/// @param[in] names The new URL.
void ActiveGame::setInfoUrl(QUrl url)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("setInfoUrl(%1)").arg(url.url()));
    m_infoUrl = std::move(url);
}

/// Set the game status information URL. The game status page provides
/// the current games state/statistics, which can be used to tell when
/// the game has finished.
///
/// @param[in] names The new URL.
void ActiveGame::setGameUrl(QUrl url)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("setGameUrl(%1)").arg(url.url()));
    m_gameUrl = std::move(url);
}

/// Do the supplied team names/abbrevs match this game. First try an
/// exact match of names, and then look for whether the class variable
/// names contain the argument names or vice versa. (I.E. TV listing
/// says 'Chorrera' but ESPN has 'Independiente de la Chorrera'.)
/// Lastly compare the abbreviations.
///
/// @param[in] names The names of the two teams.
/// @param[in] abbrevs The abbreviations of the two teams.
/// @returns whether or not a match was found.
bool ActiveGame::teamsMatch(const QStringList& names, const QStringList& abbrevs) const
{
    // Exact name matches
    if ((m_team1Normalized == names[0]) &&
        (m_team2Normalized == names[1]))
        return true;
    if ((m_team1Normalized == names[1]) &&
        (m_team2Normalized == names[0]))
        return true;

    // One name or the other is shortened
    if (((m_team1Normalized.contains(names[0])) ||
         (names[0].contains(m_team1Normalized))) &&
        ((m_team2Normalized.contains(names[1])) ||
         names[1].contains(m_team2Normalized)))
        return true;
    if (((m_team1Normalized.contains(names[1])) ||
         (names[1].contains(m_team1Normalized))) &&
        ((m_team2Normalized.contains(names[0])) ||
         names[0].contains(m_team2Normalized)))
        return true;

    // Check abbrevs
    if ((m_team1 == abbrevs[0]) && (m_team2 == abbrevs[1]))
        return true;
    return ((m_team1 == abbrevs[1]) && (m_team2 == abbrevs[0]));
}

//////////////////////////////////////////////////
///                Base Classes                ///
//////////////////////////////////////////////////

/// Does the specified time fall within -3/+1 hour from now?
///
/// @param eventStart The starting time of the sporting event.
/// @returns true if the starting time is within the specified window.
bool RecExtDataPage::timeIsClose(const QDateTime& eventStart)
{
    QDateTime now    = getNow();
    QDateTime past   = now.addSecs(-kLookBackTime);
    QDateTime future = now.addSecs( kLookForwardTime);
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("past:       %1.").arg(past.toString(Qt::ISODate)));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("eventStart: %1.").arg(eventStart.toString(Qt::ISODate)));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("future:     %1.").arg(future.toString(Qt::ISODate)));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("result is   %1.")
        .arg(((past < eventStart) && (eventStart < future)) ? "true" : "false"));
#endif
    return ((past < eventStart) && (eventStart < future));
}

/// Iterate through a json object and return the specified object.
///
/// @param object The json object to process.
/// @param path A list of successive items to look up in the
///             specified object. The last object looked up will be
///             returned to the caller.
/// @returns The object at the end of the path lookup, or an empty object.
QJsonObject RecExtDataPage::walkJsonPath(QJsonObject& object, const QStringList& path)
{
    static QRegularExpression re { R"((\w+)\[(\d+)\])" };
    QRegularExpressionMatch match;

    for (const QString& step : path)
    {
        if (step.contains(re, &match))
        {
            QString name = match.captured(1);
            int index = match.captured(2).toInt();
            if (!object.contains(name) || !object[name].isArray())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Invalid json at %1 in path %2 (not an array)")
                    .arg(name, path.join('/')));
                return {};
            }
            QJsonArray array = object[name].toArray();
            if ((array.size() < index) || !array[index].isObject())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Invalid json at %1[%2] in path %3 (invalid array)")
                    .arg(name).arg(index).arg(path.join('/')));
                return {};
            }
            object = array[index].toObject();
        }
        else
        {
            if (!object.contains(step) || !object[step].isObject())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Invalid json at %1 in path %2 (not an object)")
                    .arg(step, path.join('/')));
                return {};
            }
            object = object[step].toObject();
        }
    }
    return object;
}

/// Retrieve the specified integer from a json object.
///
/// @param[in] object The json object to process.
/// @param[in] path A list of successive items to look up in the
///                 specified object. The last object looked up will be
///                 returned to the caller.
/// @param[out] value Where to store the retrieved integer.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonInt(const QJsonObject& _object, QStringList& path, int& value)
{
    if (path.empty())
        return false;
    QString key = path.takeLast();
    QJsonObject object = _object;
    if (!path.empty())
        object = walkJsonPath(object, path);
    if (object.isEmpty() || !object.contains(key) || !object[key].isDouble())
    {
       LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("invalid key: %1.").arg(path.join('/')));
        return false;
    }
    value = object[key].toDouble();
    return true;
}

/// Retrieve the specified integer from a json object.
///
/// @param[in] object The json object to process.
/// @param[in] key A description of how to find a specific integer
///                object. This can be a single key name, or a series of
///                key names separated with slash characters.
/// @param[out] value Where to store the retrieved integer.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonInt(const QJsonObject& object, const QString& key, int& value)
{
    QStringList list = key.split('/');
    return getJsonInt(object, list, value);
}

/// Retrieve the specified string from a json object.
///
/// @param[in] object The json object to process.
/// @param[in] path A description of how to find a specific string object.
/// @param[out] value Where to store the retrieved string.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonString(const QJsonObject& _object, QStringList& path, QString& value)
{
    if (path.empty())
        return false;
    QString key = path.takeLast();
    QJsonObject object = _object;
    if (!path.empty())
        object = walkJsonPath(object, path);
    if (object.isEmpty() || !object.contains(key) || !object[key].isString())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("invalid key: %1.").arg(path.join('/')));
        return false;
    }
    value = object[key].toString();
    return true;
}

/// Retrieve the specified string from a json object.
///
/// @param[in] object The json object to process.
/// @param[in] key A description of how to find a specific string
///                object. This can be a single key name, or a series of
///                key names separated with slash characters.
/// @param[out] value Where to store the retrieved string.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonString(const QJsonObject& object, const QString& key, QString& value)
{
    QStringList list = key.split('/');
    return getJsonString(object, list, value);
}

/// Retrieve a specific object from another json object.
///
/// @param[in] object The json object to process.
/// @param[in] key The name of the object to return.
/// @param[out] value Where to store the retrieved object.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonObject(const QJsonObject& _object, QStringList& path, QJsonObject& value)
{
    if (path.empty())
        return false;
    QString key = path.takeLast();
    QJsonObject object = _object;
    if (!path.empty())
        object = walkJsonPath(object, path);
    if (object.isEmpty() || !object.contains(key) || !object[key].isObject())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("invalid key: %1.").arg(path.join('/')));
        return false;
    }
    value = object[key].toObject();
    return true;
}

/// Retrieve a specific object from another json object.
///
/// @param[in] object The json object to process.
/// @param[in] key A description of how to find a specific
///                object. This can be a single key name, or a series
///                of key names separated with slash characters.
/// @param[out] value Where to store the retrieved object.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonObject(const QJsonObject& object, const QString& key, QJsonObject& value)
{
    QStringList list = key.split('/');
    return getJsonObject(object, list, value);
}

/// Retrieve the specified array from a json object.
///
/// @param[in] object The json object to process.
/// @param[in] key The name of the array to return.
/// @param[out] value Where to store the retrieved object.
/// @returns true if the parsing was successful and a value returned.
bool RecExtDataPage::getJsonArray(const QJsonObject& object, const QString& key, QJsonArray& value)
{
    if (!object.contains(key) || !object[key].isArray())
        return false;
    value = object[key].toArray();
    return true;
}

//////////////////////////////////////////////////

/// A cache of downloaded documents.
QHash<QString,QJsonDocument> RecExtDataSource::s_downloadedJson {};

/// Clear the downloaded document cache.
void RecExtDataSource::clearCache()
{
    s_downloadedJson.clear();
}

/// Remove all diacritical marks, etc., etc., from a string leaving
/// just the base characters. This is needed for cases where TV
/// listing use diacritical marks in the sports team names, but the
/// information provider doesn't.
///
/// @param s The string to clean up.
/// @returns The cleaned up string.
static QString normalizeString(const QString& s)
{
    QString result;

    QString norm = s.normalized(QString::NormalizationForm_D);
    for (QChar c : std::as_const(norm))
    {
        switch (c.category())
        {
          case QChar::Mark_NonSpacing:
          case QChar::Mark_SpacingCombining:
          case QChar::Mark_Enclosing:
            continue;
          default:
            result += c;
        }
    }

    // Possibly needed? Haven't seen a team name with a German eszett
    // to know how they are handled by the api providers.
    //result = result.replace("ß","ss");
    return result.simplified();
}

//////////////////////////////////////////////////
///                    ESPN                    ///
//////////////////////////////////////////////////

// The URL to get the names of all the leagues in a given sport:
// https://site.api.espn.com/apis/site/v2/leagues/dropdown?sport=${sport}&limit=100
//
// The URL to get the names of all the teams in a league:
// http://site.api.espn.com/apis/site/v2/sports/${sport}/${league}/teams

// The URL to retrieve schedules and scores.
// http://site.api.espn.com/apis/site/v2/sports/${sport}/${league}/scoreboard
// http://site.api.espn.com/apis/site/v2/sports/${sport}/${league}/scoreboard?dates=20180901
// http://sports.core.api.espn.com/v2/sports/${sport}/leagues/${league}/events/${eventId}/competitions/${eventId}/status
//
// Mens College Basketball (Group 50)
//
// All teams:
// http://site.api.espn.com/apis/site/v2/sports/basketball/mens-college-basketball/teams?groups=50&limit=500
//
// This only shows teams in the  top 25:
// http://site.api.espn.com/apis/site/v2/sports/basketball/mens-college-basketball/scoreboard?date=20220126
//
// This shows all the scheduled games.
// http://site.api.espn.com/apis/site/v2/sports/basketball/mens-college-basketball/scoreboard?date=20220126&groups=50&limit=500

static const QString espnInfoUrlFmt {"http://site.api.espn.com/apis/site/v2/sports/%1/%2/scoreboard"};
static const QString espnGameUrlFmt {"http://sports.core.api.espn.com/v2/sports/%1/leagues/%2/events/%3/competitions/%3/status"};

/// A list of the ESPN status that mean the game is over.
const QList<RecExtEspnDataPage::GameStatus> RecExtEspnDataPage::kFinalStatuses {
    FINAL, FORFEIT, CANCELLED, POSTPONED, SUSPENDED,
    FORFEIT_HOME_TEAM, FORFEIT_AWAY_TEAM, ABANDONED, FULL_TIME,
    PLAY_COMPLETE, OFFICIAL_EVENT_SHORTENED, RETIRED,
    BYE, ESPNVOID, FINAL_SCORE_AFTER_EXTRA_TIME, FINAL_SCORE_AFTER_GOLDEN_GOAL,
    FINAL_SCORE_AFTER_PENALTIES, END_EXTRA_TIME, FINAL_SCORE_ABANDONED,
};

/// Parse a previously downloaded data page for a given sport. Find an
/// entry that matches the specified team names, and update the game
/// information with the starting time and a URL for retrieving the
/// score for this game.
///
/// @param game[in,out] A description of the desired game. The team
///                     names must be valid on input.
/// @returns true if a match was found.
bool RecExtEspnDataPage::findGameInfo(ActiveGame& game)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Looking for match of %1/%2")
        .arg(game.getTeam1(), game.getTeam2()));

    QJsonObject json = m_doc.object();
    if (json.isEmpty())
        return false;
    if (!json.contains("events") || !json["events"].isArray())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("malformed json document, step %1.").arg(1));
        return false;
    }

    // Process the games
    QJsonArray eventArray = json["events"].toArray();
    for (const auto& eventValue : std::as_const(eventArray))
    {
        // Process info at the game level
        if (!eventValue.isObject())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("malformed json document, step %1.").arg(2));
            continue;
        }
        QJsonObject event = eventValue.toObject();

        // Top level info for a game
        QString idStr {};
        QString dateStr {};
        QString gameTitle {};
        QString gameShortTitle {};
        if (!getJsonString(event, "id", idStr) ||
            !getJsonString(event, "date", dateStr) ||
            !getJsonString(event, "name", gameTitle) ||
            !getJsonString(event, "shortName", gameShortTitle))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("malformed json document, step %1.").arg(3));
            continue;
        }
        QStringList teamNames   = gameTitle.split(kVersusPattern);
        QStringList teamAbbrevs = gameShortTitle.split(kVersusPattern);
        if ((teamNames.size() != 2) || (teamAbbrevs.size() != 2))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("malformed json document, step %1.").arg(4));
            continue;
        }
        RecordingExtender::nameCleanup(game.getInfo(), teamNames[0], teamNames[1]);
        if (!game.teamsMatch(teamNames, teamAbbrevs))
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Found %1 at %2 (%3 @ %4). Teams don't match.")
                .arg(teamNames[0], teamNames[1],
                     teamAbbrevs[0], teamAbbrevs[1]));
            continue;
        }
        QDateTime startTime = QDateTime::fromString(dateStr, Qt::ISODate);
        if (!timeIsClose(startTime))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Found '%1 vs %2' starting time %3 wrong")
                .arg(game.getTeam1(), game.getTeam2(),
                     game.getStartTimeAsString()));
            continue;
        }

        // Found everthing we need.
        game.setAbbrevs(teamAbbrevs);
        game.setStartTime(startTime);
        game.setGameUrl(getSource()->makeGameUrl(game, idStr));

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Match: %1 at %2 (%3 @ %4), start %5.")
            .arg(game.getTeam1(), game.getTeam2(),
                 game.getAbbrev1(), game.getAbbrev2(),
                 game.getStartTimeAsString()));
        return true;
    }
    return false;
}

/// Parse the previously downloaded data page for a given game. Find
/// the current state of the game, and the current period.
/// Unfortunately for us the "completed" field on this page can't be
/// used, as it indicates that the final score of the game has gone
/// into the record books. If the game has been
/// suspended/postponed/whatever but not gone the required length of
/// time to be considered "complete", this field remains false.
///
/// @param[in] game A description of the desired game. The team
///                 names and the URL must be valid on input.
/// @return The game state for the specified game.
GameState RecExtEspnDataPage::findGameScore(ActiveGame& game)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Parsing game score for %1/%2")
        .arg(game.getTeam1(), game.getTeam2()));

    QJsonObject json = m_doc.object();
    if (json.isEmpty())
        return {};

    int period {-1};
    QString typeId;
    QString detail;
    QString description;
    if (!getJsonInt(json, "period", period) ||
        !getJsonString(json, "type/id", typeId) ||
        !getJsonString(json, "type/description", description) ||
        !getJsonString(json, "type/detail", detail))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("malformed json document, step %1.").arg(5));
        return {};
    }
    auto stateId = static_cast<GameStatus>(typeId.toInt());
    bool gameOver = kFinalStatuses.contains(stateId);

    GameState state(game, period, gameOver);
    QString extra;
    if ((description == detail) || (description == "In Progress"))
        extra = detail;
    else
        extra = QString ("%1: %2").arg(description, detail);
    state.setTextState(extra);
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("%1 at %2 (%3 @ %4), %5.")
        .arg(game.getTeam1(), game.getTeam2(),
             game.getAbbrev1(), game.getAbbrev2(), extra));
    return state;
}

/// Download the data page for a game, and do some minimal validation.
/// Cache the result so that subsequent lookups don't cause repeated
/// downloads of the same page. (The cache must be cleared
/// periodically.)
///
/// @param[in] game A description of the desired game. The team
///                 names and the URL must be valid on input.
/// @return The downloaded json document.
RecExtDataPage*
RecExtEspnDataSource::loadPage(const ActiveGame& game, const QUrl& _url)
{
    QString url = _url.url();

    // Return cached document
    if (s_downloadedJson.contains(url))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Using cached document for %1.").arg(url));
        return newPage(s_downloadedJson[url]);
    }

    QByteArray data;
    bool ok {false};
    QString scheme = _url.scheme();
    if (scheme == QStringLiteral(u"file"))
    {
        QFile file(_url.path(QUrl::FullyDecoded));
        ok = file.open(QIODevice::ReadOnly);
        if (ok)
            data = file.readAll();
    }
    else if ((scheme == QStringLiteral(u"http")) ||
             (scheme == QStringLiteral(u"https")))
    {
        ok = GetMythDownloadManager()->download(url, &data);
    }
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("\"%1\" couldn't download %2.")
            .arg(game.getTitle(), url));
        return nullptr;
    }

    QJsonParseError error {};
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error parsing %1 at offset %2: %3")
            .arg(url).arg(error.offset).arg(error.errorString()));
        return nullptr;
    }

    QJsonObject json = doc.object();
    if (json.contains("code") && json["code"].isDouble() &&
        json.contains("detail") && json["detail"].isString())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("error downloading json document, code %1, detail %2.")
            .arg(json["code"].toInt()).arg(json["detail"].toString()));
        return nullptr;
    }
    s_downloadedJson[url] = doc;
    return newPage(doc);
}

/// Create a URL for the ESPN API that is built from the various known
/// bits of data accumulated so far.
///
/// @param[in] info A data structure describing the sport/league.
/// @param[in] dt The date for which sporting events should be retrieved.
/// @returns a url to extract the list of sporting events from the ESPN API.
QUrl RecExtEspnDataSource::makeInfoUrl (const SportInfo& info, const QDateTime& dt)
{
    QUrl url {QString(espnInfoUrlFmt).arg(info.sport, info.league)};
    QUrlQuery query;
    query.addQueryItem("limit", "500");
    // Add this to get all games, otherwise only top-25 games are returned.
    if (info.league.endsWith("college-basketball"))
        query.addQueryItem("group", "50");
    if (dt.isValid())
        query.addQueryItem("dates", dt.toString("yyyyMMdd"));
    url.setQuery(query);
    return url;
}

/// Create a URL for one specific game in the ESPN API that is built
/// from the various known bits of data accumulated so far.
///
/// @param[in] game A data structure describing the specific game of interest.
/// @param[in] str The game id for this specific game.
/// @returns a url to extract the status of this specific game from the ESPN API.
QUrl RecExtEspnDataSource::makeGameUrl(const ActiveGame& game, const QString& str)
{
    SportInfo info = game.getInfo();
    QUrl gameUrl = QUrl(espnGameUrlFmt.arg(info.sport, info.league, str));
    return gameUrl;
}

/// Find the right URL for a specific recording. The data pages are
/// grouped by the day they start (in local time), but the retrieval
/// service assumes that URLs are specified with UTC. For example,
/// retrieving ".../scoreboard" at 19:59 EDT (23:59Z) returns the
/// games for that day, but at 20:00 EDT (00:00Z) it retrieves the
/// games for the next day. The form ".../scoreboard?dates=yyyMMdd"
/// has to be used to select the correct data page. This can mean that
/// two games being simultaneously recorded use different URLS if one
/// started before 00:00Z and the other started after.
///
/// @param[in] game A description of the desired game. The team
///                  names and the URL must be valid on input.
/// @param[in,out] info A structure describing this data endpoint.
/// @returns The URL to use for data on this specific game.
QUrl RecExtEspnDataSource::findInfoUrl(ActiveGame& game, SportInfo& info)
{
    // Find game with today's date (in UTC)
    // Is the starting time close to now?
    QDateTime now = MythDate::current();
    game.setInfoUrl(makeInfoUrl(info, now));
    RecExtDataPage* page = loadPage(game, game.getInfoUrl());
    if (!page)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Couldn't load %1").arg(game.getInfoUrl().url()));
        return {};
    }
    if (page->findGameInfo(game))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Found game '%1 vs %2' at %3")
            .arg(game.getTeam1(), game.getTeam2(), game.getStartTimeAsString()));
        return game.getInfoUrl();
    }

    // Find game with yesterdays's date (in UTC)
    // Handles evening games that start after 00:00 UTC. E.G. an 8pm EST football game.
    // Is the starting time close to now?
    game.setInfoUrl(makeInfoUrl(info, now.addDays(-1)));
    page = loadPage(game, game.getInfoUrl());
    if (!page)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Couldn't load %1").arg(game.getInfoUrl().url()));
        return {};
    }
    if (page->findGameInfo(game))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Found game '%1 vs %2' at %3")
            .arg(game.getTeam1(), game.getTeam2(), game.getStartTimeAsString()));
        return game.getInfoUrl();
    }

    // Find game with tomorrow's date (in UTC)
    // E.G. Handles
    // Is the starting time close to now?
    game.setInfoUrl(makeInfoUrl(info, now.addDays(1)));
    page = loadPage(game, game.getInfoUrl());
    if (!page)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Couldn't load %1").arg(game.getInfoUrl().url()));
        return {};
    }
    if (page->findGameInfo(game))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Found game '%1 vs %2' at %3")
            .arg(game.getTeam1(), game.getTeam2(), game.getStartTimeAsString()));
        return game.getInfoUrl();
    }
    return {};
}

//////////////////////////////////////////////////
///                     MLB                    ///
//////////////////////////////////////////////////

// The MLB API is free for individual, non-commercial use. See
// http://gdx.mlb.com/components/copyright.txt
//
// Working queryable version of the API:
// https://beta-statsapi.mlb.com/docs/
//
// For schedule information:
// https://statsapi.mlb.com/api/v1/schedule?sportId=1&date=2021-09-18
// https://statsapi.mlb.com/api/v1/schedule?sportId=1&startDate=2021-09-18&endDate=2021-09-20"
//
// For game information:
// https://statsapi.mlb.com/api/v1.1/game/{game_pk}/feed/live
// where the game_pk comes from the schedule data.
//
// You can request extra data be returned by asking for "hydrations"
// (additional data) to be added to the response. The list of
// available hydrations for any API can be retrieved by adding
// "hydrate=hydrations" to the URL. For example:
// https://statsapi.mlb.com/api/v1/schedule?sportId=1&hydrate=hydrations&date=2021-09-18"
// https://statsapi.mlb.com/api/v1/schedule?sportId=1&hydrate=team&startDate=2021-09-18&endDate=2021-09-20"

/// Parse a single game from the returned MLB schedule page.
///
/// @param[in] gameObject The json object on the MLB schedule page
///                       that describes a single game.
/// @param[in,out] game   The game data structure to be filled in.
/// @returns true if the json was successfully parsed.
bool RecExtMlbDataPage::parseGameObject(const QJsonObject& gameObject,
                                        ActiveGame& game)
{
    QString dateStr {};
    QString gameLink {};
    QStringList teamNames {"", ""};
    QStringList teamAbbrevs { "", ""};
    if (!getJsonString(gameObject, "gameDate",  dateStr) ||
        !getJsonString(gameObject, "link", gameLink) ||
        !getJsonString(gameObject, "teams/home/team/name", teamNames[0]) ||
        !getJsonString(gameObject, "teams/away/team/name", teamNames[1]) ||
        !getJsonString(gameObject, "teams/home/team/abbreviation", teamAbbrevs[0]) ||
        !getJsonString(gameObject, "teams/away/team/abbreviation", teamAbbrevs[1]))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("malformed json document, step %1.").arg(1));
        return false;
    }

    RecordingExtender::nameCleanup(game.getInfo(), teamNames[0], teamNames[1]);
    bool success = game.teamsMatch(teamNames, teamAbbrevs);
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Found: %1 at %2 (%3 @ %4), starting %5. (%6)")
        .arg(teamNames[0], teamNames[1],
             teamAbbrevs[0], teamAbbrevs[1], dateStr,
             success ? "Success" : "Teams don't match"));
    if (!success)
        return false;

    // Found everthing we need.
    game.setAbbrevs(teamAbbrevs);
    game.setGameUrl(getSource()->makeGameUrl(game, gameLink));
    game.setStartTime(QDateTime::fromString(dateStr, Qt::ISODate));
    return true;
}

/// Parse a previously downloaded data page for a given sport. Find an
/// entry that matches the specified team names, and update the game
/// information with the starting time and a URL for retrieving the
/// score for this game.
///
/// @param game[in,out] A description of the desired game. The team
///                     names must be valid on input.
/// @returns true if a match was found.
bool RecExtMlbDataPage::findGameInfo(ActiveGame& game)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Looking for match of %1/%2")
        .arg(game.getTeam1(), game.getTeam2()));

    QJsonObject json = m_doc.object();
    if (json.isEmpty())
        return false;

    QJsonArray datesArray;
    if (!getJsonArray(json, "dates", datesArray))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("malformed json document, step %1.").arg(1));
        return false;
    }

    // Process each of the three dates
    for (const auto& dateValue : std::as_const(datesArray))
    {
        if (!dateValue.isObject())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("malformed json document, step %1.").arg(2));
            continue;
        }
        QJsonObject dateObject = dateValue.toObject();

        QJsonArray gamesArray;
        if (!getJsonArray(dateObject, "games", gamesArray))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("malformed json document, step %1.").arg(3));
            continue;
        }

        // Process each game on a given date
        for (const auto& gameValue : std::as_const(gamesArray))
        {
            if (!gameValue.isObject())
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("malformed json document, step %1.").arg(4));
                continue;
            }
            QJsonObject gameObject = gameValue.toObject();

            if (!parseGameObject(gameObject, game))
                continue;
            bool match = timeIsClose(game.getStartTime());
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Found '%1 vs %2' starting %3 (%4)")
                .arg(game.getTeam1(), game.getTeam2(), game.getStartTimeAsString(),
                     match ? "match" : "keep looking"));
            if (!match)
                continue;
            return true;
        }
    }
    return false;
}

/// Parse the previously downloaded data page for a given game. Find
/// the current state of the game, and the current period.
///
/// @param[in] game A description of the desired game. The team
///                 names and the URL must be valid on input.
/// @return The game state for the specified game.
GameState RecExtMlbDataPage::findGameScore(ActiveGame& game)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Parsing game score for %1/%2")
        .arg(game.getTeam1(), game.getTeam2()));

    QJsonObject json = m_doc.object();
    if (json.isEmpty())
        return {};

    int period {-1};
    QString abstractGameState;
    QString detailedGameState;
    QString inningState;
    QString inningOrdinal;
    if (!getJsonString(json, "gameData/status/abstractGameState", abstractGameState) ||
        !getJsonString(json, "gameData/status/detailedState", detailedGameState))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("malformed json document (%d)").arg(1));
        return {};
    }

     if (detailedGameState != "Scheduled")
     {
         if (!getJsonInt(   json, "liveData/linescore/currentInning", period) ||
             !getJsonString(json, "liveData/linescore/currentInningOrdinal", inningOrdinal) ||
             !getJsonString(json, "liveData/linescore/inningState", inningState))
         {
             LOG(VB_GENERAL, LOG_INFO, LOC +
                 QString("malformed json document (%d)").arg(2));
             return {};
         }
     }

    bool gameOver = (abstractGameState == "Final") ||
        detailedGameState.contains("Suspended");
    GameState state = GameState(game.getTeam1(), game.getTeam2(),
                                game.getAbbrev1(), game.getAbbrev2(),
                                period, gameOver);
    QString extra;
    if (gameOver)
        extra = "game over";
    else if (detailedGameState == "In Progress")
        extra = QString("%1 %2").arg(inningState, inningOrdinal);
    else
        extra = detailedGameState;
    state.setTextState(extra);
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("%1 at %2 (%3 @ %4), %5.")
        .arg(game.getTeam1(), game.getTeam2(),
             game.getAbbrev1(), game.getAbbrev2(), extra));
    return state;
}

/// Download the data page for a game, and do some minimal validation.
/// Cache the result so that subsequent lookups don't cause repeated
/// downloads of the same page. (The cache must be cleared
/// periodically.)
///
/// @param[in] game A description of the desired game. The team
///                 names and the URL must be valid on input.
/// @return The downloaded json document.
RecExtDataPage*
RecExtMlbDataSource::loadPage(const ActiveGame& game, const QUrl& _url)
{
    QString url = _url.url();

    // Return cached document
    if (s_downloadedJson.contains(url))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Using cached document for %1.").arg(url));
        return newPage(s_downloadedJson[url]);
    }

    QByteArray data;
    bool ok {false};
    QString scheme = _url.scheme();
    if (scheme == QStringLiteral(u"file"))
    {
        QFile file(_url.path(QUrl::FullyDecoded));
        ok = file.open(QIODevice::ReadOnly);
        if (ok)
            data = file.readAll();
    }
    else if ((scheme == QStringLiteral(u"http")) ||
             (scheme == QStringLiteral(u"https")))
    {
        ok = GetMythDownloadManager()->download(url, &data);
    }
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("\"%1\" couldn't download %2.")
            .arg(game.getTitle(), url));
        return nullptr;
    }

    QJsonParseError error {};
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error parsing %1 at offset %2: %3")
            .arg(url).arg(error.offset).arg(error.errorString()));
        return nullptr;
    }
    s_downloadedJson[url] = doc;
    return newPage(doc);
}

/// Create a URL for the MLB API that is built from the various known
/// bits of data accumulated so far.
///
/// @param info A data structure describing the sport/league.
/// @param dt The date for which sporting events should be retrieved.
/// @returns a URL to extract the list of sporting events from the MLB API.
QUrl RecExtMlbDataSource::makeInfoUrl ([[maybe_unused]] const SportInfo& info,
                                       const QDateTime& dt)
{
    if (!dt.isValid())
        return {};

    QDateTime yesterday = dt.addDays(-1);
    QDateTime tomorrow = dt.addDays(+1);
    QUrl url {"https://statsapi.mlb.com/api/v1/schedule"};
    QUrlQuery query;
    query.addQueryItem("sportId",   "1");
    query.addQueryItem("hydrate",   "team");
    query.addQueryItem("startDate", QString("%1").arg(yesterday.toString("yyyy-MM-dd")));
    query.addQueryItem("endDate",   QString("%1").arg(tomorrow.toString("yyyy-MM-dd")));
    url.setQuery(query);
    return url;
}

/// Create a URL for one specific game in the MLB API that is built
/// from the various known bits of data accumulated so far.
///
/// @param game A data structure describing the specific game of interest.
/// @param str The game id for this specific game.
/// @returns a URL to extract the status of this specific game from the MLB API.
QUrl RecExtMlbDataSource::makeGameUrl (const ActiveGame& game, const QString& str)
{
    QUrl gameUrl = game.getInfoUrl();
    gameUrl.setPath(str);
    gameUrl.setQuery(QString());
    return gameUrl;
}

/// Find the right URL for a specific recording. The data pages are
/// grouped by the day they start (in local time), but the retrieval
/// service operates using UTC. For example, retrieving
/// ".../scoreboard" at 19:59EDT (23:59Z) returns the games for that
/// day, but at 20:00EDT (00:00Z) it retrieves the games for the next
/// day. The form ".../scoreboard?dates=yyyMMdd" has to be used to
/// select the correct data page.
///
/// @param[in] game A description of the desired game. The team
///                  names and the URL must be valid on input.
/// @param info A structure describing this data endpoint.
/// @returns The URL to use for data on this specific game.
QUrl RecExtMlbDataSource::findInfoUrl(ActiveGame& game, SportInfo& info)
{
    // Find game with today's date (in UTC)
    // Is the starting time close to now?
    QDateTime now = MythDate::current();
    game.setInfoUrl(makeInfoUrl(info, now));
    RecExtDataPage* page = loadPage(game, game.getInfoUrl());
    if (!page)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Couldn't load %1").arg(game.getInfoUrl().url()));
        return {};
    }
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Loaded page %1").arg(game.getInfoUrl().url()));
    if (page->findGameInfo(game))
        return game.getGameUrl();

    return {};
}

//////////////////////////////////////////////////
///             RecordingExtender              ///
//////////////////////////////////////////////////

QMutex RecordingExtender::s_createLock {};
RecordingExtender* RecordingExtender::s_singleton {nullptr};

RecordingExtender::~RecordingExtender()
{
    clearDownloadedInfo();
}

/// Create an instance of the RecordingExtender if necessary, and add
/// this recording to the list of new recordings. This function is
/// called by the scheduler for all new recordings, but only does
/// anything if the recording is marked to be auto extended.
///
/// @note This function runs on the scheduler thread. It defers all
/// work to the Recording Extender thread.
///
/// @param scheduler A pointer to the scheduler object.
/// @param ri The recording that just started.
void RecordingExtender::create (Scheduler *scheduler, RecordingInfo& ri)
{
    RecordingRule *rr = ri.GetRecordingRule();
    if (rr->m_autoExtend == AutoExtendType::None)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Recording of %1 at %2 not marked for auto extend.")
            .arg(ri.GetTitle(), ri.GetScheduledStartTime(MythDate::ISODate)));
        return;
    }
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Adding %1 at %2 to new recordings list.")
        .arg(ri.GetTitle(), ri.GetScheduledStartTime(MythDate::ISODate)));

    QMutexLocker lock(&s_createLock);
    if (!s_singleton)
    {
        s_singleton = new RecordingExtender();
        s_singleton->m_scheduler = scheduler;
        s_singleton->start();
        s_singleton->moveToThread(s_singleton->qthread());
    }
    s_singleton->addNewRecording(ri.GetRecordingID());
};

/// Add an item to the list of new recordings. Handles locking of the
/// new recordings list.
///
/// @param recordedID The recording ID of the recording that just started.
void RecordingExtender::addNewRecording(int recordedID)
{
    QMutexLocker lock(&m_newRecordingsLock);
    m_newRecordings.append(recordedID);
}

/// Create a RecExtDataSource object for the specified service. This
/// will never produce the base class object, but will only produce
/// one of the subclass objects.
///
/// @param[in] parent A pointer to the RecordingExtender object that
///                   created this data source.
/// @param[in] type   An identifier for the service used to check the
///                   game status.
RecExtDataSource* RecordingExtender::createDataSource(AutoExtendType type)
{
    switch (type)
    {
      default:
        return nullptr;
      case AutoExtendType::ESPN:
        return new RecExtEspnDataSource(this);
      case AutoExtendType::MLB:
        return new RecExtMlbDataSource(this);
    }
}

/// Retrieve the db record for a sporting event on a specific
/// provider.  This function handles regex matching
/// between program listings and database entries.
///
/// If the title contains a year (i.e. four consecutive digits) then
/// the year must be this year in order to match. Handling the year
/// this way simplifies all the regular expressions, eliminating the
/// need for each one to allow for a starting or ending year.
///
/// @param title The program title of the sporting event. I.E. "MLB
///              Baseball", "2021 World Series", or "Superbowl LVI".
/// @param type An identifier for the service used to check the
///             game status.
/// @param infoList If the function returns true, this contains a list
///                 of the databases record for the specified sporting
///                 event and provider.  Unchanged otherwise.
///                 Everything other than FIFA Soccer Qualification
///                 Matches will likely return a single entry.
/// @return true if the title matches a known sport.
bool RecordingExtender::findKnownSport(const QString& _title,
                                       AutoExtendType type,
                                       SportInfoList& infoList) const
{
    static const QRegularExpression year {R"(\d{4})"};
    QRegularExpressionMatch match;
    QString title = _title;
    if (title.contains(year, &match))
    {
        bool ok {false};
        int matchYear = match.captured().toInt(&ok);
        int thisYear = m_forcedYearforTesting
            ? m_forcedYearforTesting
            : QDateTime::currentDateTimeUtc().date().year();
        // FIFA Qualifiers can be in the year before the tournament.
        if (!ok || ((matchYear != thisYear) && (matchYear != thisYear+1)))
            return false;
        title = title.remove(match.capturedStart(), match.capturedLength());
    }
    title = title.simplified();
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Looking for %1 title '%2")
        .arg(toString(type), title));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sl.title, api.provider, api.key1, api.key2" \
        " FROM sportslisting sl " \
        " INNER JOIN sportsapi api ON sl.api = api.id" \
        " WHERE api.provider = :PROVIDER AND :TITLE REGEXP sl.title");
    query.bindValue(":PROVIDER", static_cast<uint8_t>(type));
    query.bindValue(":TITLE", title);
    if (!query.exec())
    {
        MythDB::DBError("sportsapi() -- findKnownSport", query);
        return false;
    }
    while (query.next())
    {
        SportInfo info;

        info.showTitle    = query.value(0).toString();
        info.dataProvider = type;
        info.sport        = query.value(2).toString();
        info.league       = query.value(3).toString();
        infoList.append(info);

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Info: '%1' matches '%2' '%3' '%4' '%5'")
            .arg(title, toString(info.dataProvider), info.showTitle,
                 info.sport, info.league));
    }
    return !infoList.isEmpty();
}

///  Clear all downloaded info.
void RecordingExtender::clearDownloadedInfo()
{
    RecExtDataSource::clearCache();
}

// Parse a single string. First split it into parts on a semi-colon or
// 'period space', and then selectively check those parts for the
// pattern "A vs B".
static bool parseProgramString (const QString& string,
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                                int limit,
#else
                                qsizetype limit,
#endif
                                QString& team1, QString& team2)
{
    QString lString = string;
    QStringList parts = lString.replace("vs.", "vs").split(kSentencePattern);
    for (int i = 0; i < std::min(limit,parts.size()); i++)
    {
        QStringList words = parts[i].split(kVersusPattern);
        if (words.size() == 2)
        {
            team1 = words[0].simplified();
            team2 = words[1].simplified();
            return true;
        }
    }
    return false;
}

/// Parse a RecordingInfo to find the team names. Depending on the
/// guide data source, this can be found either in the subtitle or in
/// the description field.
///
/// @param[in] ri The RecordingInfo to parse.
/// @param[out] team1 The name of the first team.
/// @param[out] team2 The name of the second team.
/// @return true if the team names could be determined.
bool RecordingExtender::parseProgramInfo (const QString& subtitle, const QString& description,
                                          QString& team1, QString& team2)
{
    if (parseProgramString(subtitle, 2, team1, team2))
        return true;
    if (parseProgramString(description, 1, team1, team2))
        return true;
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("can't find team names in subtitle or description '%1'").arg(description));
    return false;
}

/// Quick helper function for printing recording rule numbers. If
/// there is a parent rule this prints two numbers, otherwise it
/// prints one.
///
/// @param[in] rr The recording rule to print.
/// @returns a string representing this rule.
QString RecordingExtender::ruleIdAsString(const RecordingRule *rr)
{
    if (rr->m_parentRecID)
        return QString("%1->%2").arg(rr->m_parentRecID).arg(rr->m_recordID);
    return QString::number(rr->m_recordID);
}

/// Clean up a single team name for comparison against the ESPN
/// API. This means normalizing the Unicode, removing any diacritical
/// marks, and removing any extraneous white space. For soccer,
/// however, it also means removing a bunch of common sets of initials
/// like "FC (Football Club)", "SC (Sports Club)", etc. that don't
/// appear in the ESPN API.
///
/// @param[in] info A data structure defining the current sport.
/// @param[in,out] name The sports team name to clean up.
void RecordingExtender::nameCleanup(const SportInfo& info, QString& name)
{
    name = normalizeString(name);
    name = name.simplified();

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Start: %1").arg(name));

    // Ask the database for all the applicable cleanups
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sc.name, sc.pattern, sc.nth, sc.replacement" \
        " FROM sportscleanup sc " \
        " WHERE (provider=0 or provider=:PROVIDER) " \
        "   AND (key1='all' or key1=:SPORT) " \
        "   AND (:NAME REGEXP pattern)" \
        " ORDER BY sc.weight");
    query.bindValue(":PROVIDER", static_cast<uint8_t>(info.dataProvider));
    query.bindValue(":SPORT", info.sport);
    query.bindValue(":NAME", name);
    if (!query.exec())
    {
        MythDB::DBError("sportscleanup() -- main query", query);
        return;
    }

    // Now apply each cleanup.
    while (query.next())
    {
        QString patternName = query.value(0).toString();
        QString patternStr = query.value(1).toString();
        int     patternField = query.value(2).toInt();
        QString replacement = query.value(3).toString();

        QString original = name;
        QString tag {"no match"};
        QRegularExpressionMatch match;
        // Should always be true....
        if (name.contains(QRegularExpression(patternStr), &match) &&
            match.hasMatch())
        {
            QString capturedText = match.captured(patternField);
            name = name.replace(match.capturedStart(patternField),
                                match.capturedLength(patternField),
                                replacement);
            name = name.simplified();
            if (name.isEmpty())
            {
                name = original;
                tag = "fail";
            }
            else
            {
                tag = QString("matched '%1'").arg(capturedText);
            }
        }
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("pattern '%1', %2, now '%3'")
            .arg(patternName, tag, name));
    }
}

/// Clean up two team names for comparison against the ESPN API.
///
/// @sa RecordingExtender::nameCleanup(const SportInfo& info, QString& name)
///
/// @param[in] info A data structure defining the current sport.
/// @param[in,out] name1 The first sports team name to clean up.
/// @param[in,out] name2 The second sports team name to clean up.
void RecordingExtender::nameCleanup(const SportInfo& info, QString& name1, QString& name2)
{
    nameCleanup(info, name1);
    if (!name2.isEmpty())
        nameCleanup(info, name2);
}

/// Stop the current recording early. The main idea behind having this
/// function is to catch games that are called on account of rain,
/// etc, and not record whatever random programming the network shows
/// after the game is postponed.
///
/// @param ri A pointer to the RecordingInfo for the recording to be
///           extended.
/// @param rr A pointer to the RecordingRule for the recording to be
///           extended.
/// @param game A reference to the current sporting event.
void RecordingExtender::finishRecording(
    const RecordingInfo *ri, RecordingRule *rr, ActiveGame const& game)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Recording %1 rule %2 for '%3 @ %4' has finished. Stop recording.")
        .arg(ri->GetRecordingID())
        .arg(ruleIdAsString(rr), game.getTeam1(), game.getTeam2()));

    MythEvent me(QString("STOP_RECORDING %1 %2")
                 .arg(ri->GetChanID())
                 .arg(ri->GetRecordingStartTime(MythDate::ISODate)));
    gCoreContext->dispatch(me);
}

/// Extend the current recording by XX minutes. The first time this is
/// done it creates a new override recording.
///
/// @param ri A pointer to the RecordingInfo for the recording to be
///           extended.
/// @param rr A pointer to the RecordingRule for the recording to be
///           extended.
/// @param game A reference to the current sporting event.
void RecordingExtender::extendRecording(
    const RecordingInfo *ri, RecordingRule *rr, const ActiveGame& game)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Recording %1 rule %2 for '%3 @ %4' scheduled to end soon. Extending recording.")
        .arg(ri->GetRecordingID())
        .arg(ruleIdAsString(rr), game.getTeam1(), game.getTeam2()));

    // Create an override to make it easy to clean up later.
    if (rr->m_type != kOverrideRecord)
    {
        rr->MakeOverride();
        rr->m_type = kOverrideRecord;
    }
    static const QString ae {"(Auto Extend)"};
    rr->m_subtitle = rr->m_subtitle.startsWith(ae)
        ? rr->m_subtitle
        : ae + ' ' + rr->m_subtitle;

    // Update the recording end time.  The m_endOffset field is the
    // one that is used by the scheduler for timing.  The others are
    // only updated for consistency.
    rr->m_endOffset += kExtensionTime.count();
    QDateTime oldDt = ri->GetRecordingEndTime();
    QDateTime newDt = oldDt.addSecs(kExtensionTimeInSec);
    rr->m_enddate = newDt.date();
    rr->m_endtime = newDt.time();

    // Update the RecordingRule and Save/Apply it.
    if (!rr->Save(true))
    {
        // Oops. Maybe the backend crashed and there's an old override
        // recording already in the table?
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Recording %1, couldn't save override rule for '%2 @ %3'.")
            .arg(ri->GetRecordingID()).arg(game.getTeam1(), game.getTeam2()));
        return;
    }

    // Debugging
    bool exists = m_overrideRules.contains(rr->m_recordID);
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Recording %1, %2 override rule %3 for '%4 @ %5' ending %6 -> %7.")
        .arg(ri->GetRecordingID())
        .arg(exists ? "updated" : "created",
             ruleIdAsString(rr), game.getTeam1(), game.getTeam2(),
             oldDt.toString(Qt::ISODate), newDt.toString(Qt::ISODate)));

    // Remember the new rule number for later cleanup.
    if (!exists)
        m_overrideRules.append(rr->m_recordID);
}

/// Log that this recording hasn't changed.
///
/// @param ri A pointer to the RecordingInfo for the recording to be
///           extended.
/// @param rr A pointer to the RecordingRule for the recording to be
///           extended.
/// @param game A reference to the current sporting event.
void RecordingExtender::unchangedRecording(
    const RecordingInfo *ri, RecordingRule *rr, const ActiveGame& game)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Recording %1 rule %2 for '%3 @ %4' ends %5.")
        .arg(ri->GetRecordingID())
        .arg(ruleIdAsString(rr), game.getTeam1(), game.getTeam2(),
             ri->GetRecordingEndTime().toString(Qt::ISODate)));
}

/// Delete the list of the override rules that have been created by
/// this instance of RecordingExtender. No need to delete the actual
/// rules, as they'll be cleaned up by the housekeeping code.
void RecordingExtender::expireOverrides()
{
    m_overrideRules.clear();
}

/// Process the list of newly started sports recordings. Create an
/// ActiveGame structure for this recording, and determine the URL to
/// get data for this particular game.
void RecordingExtender::processNewRecordings ()
{
    while (true)
    {
        QMutexLocker locker (&m_newRecordingsLock);
        if (m_newRecordings.isEmpty())
            break;
        int recordedID = m_newRecordings.takeFirst();
        locker.unlock();

        // Have to get this from the scheduler, otherwise we never see
        // the actual recording state.
        // WE OWN THIS POINTER.
        RecordingInfo *ri = m_scheduler->GetRecording(recordedID);
        if (nullptr == ri)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Couldn't get recording %1 from scheduler")
                .arg(recordedID));
            continue;
        }

        if (!ValidRecordingStatus(ri->GetRecordingStatus()))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Invalid status for '%1 : %2', status %3.")
                .arg(ri->GetTitle(), ri->GetSubtitle(),
                     RecStatus::toString(ri->GetRecordingStatus())));
	    delete ri;
            continue;
        }
        RecordingRule *rr = ri->GetRecordingRule(); // owned by ri

        SportInfoList infoList;
        if (!findKnownSport(ri->GetTitle(), rr->m_autoExtend, infoList))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Unknown sport '%1' for provider %2")
                .arg(ri->GetTitle(), toString(rr->m_autoExtend)));
            delete ri;
            continue;
        }

        auto* source = createDataSource(rr->m_autoExtend);
        if (!source)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("unable to create data source of type %1.")
                .arg(toString(rr->m_autoExtend)));
            delete ri;
            continue;
        }

        // Build the game data structure
        ActiveGame game(recordedID, ri->GetTitle());
        QString team1;
        QString team2;
        if (!parseProgramInfo(ri->GetSubtitle(), ri->GetDescription(),
                              team1, team2))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Unable to find '%1 : %2', provider %3")
                .arg(ri->GetTitle(), ri->GetSubtitle(),
                     toString(rr->m_autoExtend)));
            delete source;
            delete ri;
            continue;
        }
        game.setTeams(team1, team2);

        // Now try each of the returned sport APIs
        bool found {false};
        for (auto it = infoList.begin(); !found && it != infoList.end(); it++)
        {
            SportInfo info = *it;
            game.setInfo(info);
            nameCleanup(info, team1, team2);
            game.setTeamsNorm(team1, team2);

            source->findInfoUrl(game, info);
            if (game.getGameUrl().isEmpty())
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("unable to find data page for recording '%1 : %2'.")
                    .arg(ri->GetTitle(), ri->GetSubtitle()));
                continue;
            }
            found = true;
        }

        if (found)
            m_activeGames.append(game);
        delete source;
        delete ri;
    }
}

/// Process the currently active sports recordings. Query provider for
/// current game status. If the game finished early, stop it. If the
/// game is still going and the recording is about to end, extend the
/// recording.  get data for this particular game.
void RecordingExtender::processActiveRecordings ()
{
    for (auto it = m_activeGames.begin(); it != m_activeGames.end(); )
    {
        ActiveGame game = *it;
        // Have to get this from the scheduler, otherwise we never see
        // the change from original to override recording rule.
        // WE OWN THIS POINTER.
        RecordingInfo *_ri = m_scheduler->GetRecording(game.getRecordedId());
        if (nullptr == _ri)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Couldn't get recording %1 from scheduler")
                .arg(game.getRecordedId()));
            it = m_activeGames.erase(it);
            continue;
        }

        // Simplify memory management
        auto ri = std::make_unique<RecordingInfo>(*_ri);
        delete _ri;

        if (!ValidRecordingStatus(ri->GetRecordingStatus()))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Invalid status for '%1 : %2', status %3.")
                .arg(ri->GetTitle(), ri->GetSubtitle(),
                     RecStatus::toString(ri->GetRecordingStatus())));
            it = m_activeGames.erase(it);
            continue;
        }

        RecordingRule *rr = ri->GetRecordingRule(); // owned by ri
        auto* source = createDataSource(rr->m_autoExtend);
        if (nullptr == source)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Couldn't create source of type %1")
                .arg(toString(rr->m_autoExtend)));
            it++;
            delete source;
            continue;
        }
        auto* page = source->loadPage(game, game.getGameUrl());
        if (nullptr == page)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Couldn't load source %1, teams %2 and %3, url %4")
                .arg(toString(rr->m_autoExtend), game.getTeam1(), game.getTeam2(),
                     game.getGameUrl().url()));
            it++;
            delete source;
            continue;
        }
        auto gameState = page->findGameScore(game);
        if (!gameState.isValid())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Game state for source %1, teams %2 and %3 is invalid")
                .arg(toString(rr->m_autoExtend), game.getTeam1(), game.getTeam2()));
            it++;
            delete source;
            continue;
        }
        if (gameState.isFinished())
        {
            finishRecording(ri.get(), rr, game);
            it = m_activeGames.erase(it);
            delete source;
            continue;
        }
        if (ri->GetScheduledEndTime() <
            MythDate::current().addSecs(kExtensionTimeInSec))
        {
            extendRecording(ri.get(), rr, game);
            it++;
            delete source;
            continue;
        }
        unchangedRecording(ri.get(), rr, game);
        it++;
        delete source;
    }
}

/// Is there any remaining work? Check for both newly created
/// recording and for active recordings.
void RecordingExtender::checkDone ()
{
    QMutexLocker lock1(&s_createLock);
    QMutexLocker lock2(&m_newRecordingsLock);

    if (m_newRecordings.empty() && m_activeGames.empty())
    {
        m_running = false;
        s_singleton = nullptr;
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Nothing left to do. Exiting."));
        return;
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("%1 new recordings, %2 active recordings, %3 overrides.")
        .arg(m_newRecordings.size()).arg(m_activeGames.size())
        .arg(m_overrideRules.size()));
}

///  The main execution loop for the Recording Extender.
void RecordingExtender::run()
{
    RunProlog();

    while (m_running)
    {
        usleep(kExtensionTime); // cppcheck-suppress usleepCalled

        clearDownloadedInfo();
        processNewRecordings();
        processActiveRecordings();

        checkDone();
    }

    expireOverrides();

    RunEpilog();
    quit();
}
