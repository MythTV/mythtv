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

#ifndef RECORDING_EXTENDER_H_
#define RECORDING_EXTENDER_H_

#include <memory>

#include <QMutex>
#include <QJsonDocument>

#include "libmythbase/mthread.h"
#include "libmythtv/recordingrule.h"

class Scheduler;

//
// Database info
//
struct SportInfo
{
    QString showTitle; // Title in listings (RE match)
    AutoExtendType dataProvider {AutoExtendType::None};
    QString sport;
    QString league;
};
using SportInfoList = QList<SportInfo>;

class ActiveGame
{
  public:
    ActiveGame(int recordedid, QString title) :
        m_recordedid(recordedid), m_title(std::move(title)) {}
    ActiveGame(int recordedid, QString title, SportInfo info) :
        m_recordedid(recordedid), m_title(std::move(title)),
        m_info(std::move(info)) {}
    int       getRecordedId()  const { return m_recordedid; }
    QString   getTitle()       const { return m_title; }
    SportInfo getInfo()        const { return m_info; }
    QString   getTeam1()       const { return m_team1; }
    QString   getTeam2()       const { return m_team2; }
    QString   getTeam1Norm()   const { return m_team1Normalized; }
    QString   getTeam2Norm()   const { return m_team2Normalized; }
    QString   getAbbrev1()     const { return m_abbrev1; }
    QString   getAbbrev2()     const { return m_abbrev2; }
    QUrl      getInfoUrl()     const { return m_infoUrl; }
    QUrl      getGameUrl()     const { return m_gameUrl; }
    QDateTime getStartTime()   const { return m_startTime; }
    QString   getStartTimeAsString() const
        { return m_startTime.toString(Qt::ISODate); }

    void      setInfo(const SportInfo& info) { m_info = info; }
    void      setTeams(QString team1, QString team2)
        { m_team1 = std::move(team1); m_team2 = std::move(team2); }
    void      setTeamsNorm(QString team1, QString team2)
        { m_team1Normalized = std::move(team1); m_team2Normalized = std::move(team2); }
    void      setAbbrev1(const QString& abbrev) { m_abbrev1 = abbrev; }
    void      setAbbrev2(const QString& abbrev) { m_abbrev2 = abbrev; }
    void      setAbbrevs(QStringList abbrevs)
        { m_abbrev1 = abbrevs[0]; m_abbrev2 = abbrevs[1]; }
    void      setInfoUrl(QUrl url);
    void      setGameUrl(QUrl url);
    void      setStartTime(const QDateTime& time) { m_startTime = time; }

    bool isValid() const
    {
        return !m_team1.isEmpty() && !m_team2.isEmpty() &&
            m_startTime.isValid() && m_infoUrl.isValid();
    }

    bool teamsMatch(const QStringList& names, const QStringList& abbrevs) const;

  private:
    int            m_recordedid {0};
    QString        m_title;
    SportInfo      m_info;

    QString        m_team1;
    QString        m_team2;
    QString        m_team1Normalized;
    QString        m_team2Normalized;
    QString        m_abbrev1;
    QString        m_abbrev2;
    QUrl           m_infoUrl;
    QUrl           m_gameUrl;
    QDateTime      m_startTime;
};

//
// The status of a single game.
//
class GameState
{
  public:
    GameState() = default;
    GameState(const QString& n1, const QString& n2,
              const QString& a1, const QString& a2,
              int p, bool finished) :
        m_team1(n1.simplified()), m_team2(n2.simplified()),
        m_abbrev1(a1.simplified()), m_abbrev2(a2.simplified()),
        m_period(p), m_finished(finished) {};
    GameState(const ActiveGame& game, int p, bool finished) :
        m_team1(game.getTeam1()), m_team2(game.getTeam2()),
        m_abbrev1(game.getAbbrev1()), m_abbrev2(game.getAbbrev2()),
        m_period(p), m_finished(finished) {};
    bool isValid() {return !m_team1.isEmpty() && !m_team2.isEmpty();};
    QString getTeam1() { return m_team1; }
    QString getTeam2() { return m_team2; }
    QString getAbbrev1() { return m_abbrev1; }
    QString getAbbrev2() { return m_abbrev2; }
    QString getTextState() { return m_textState; }
    void setTextState(QString text) { m_textState = std::move(text); }
    int getPeriod() const { return m_period; }
    bool isFinished() const { return m_finished; }
    bool matchName(const QString& a, const QString& b)
        { return (((a == m_team1) && (b == m_team2)) ||
                  ((a == m_team2) && (b == m_team1))); }
    bool matchAbbrev(const QString& a, const QString& b)
        { return (((a == m_abbrev1) && (b == m_abbrev2)) ||
                  ((a == m_abbrev2) && (b == m_abbrev1))); }
    bool match(const QString& team1, const QString& team2)
        {
            if (matchName(team1, team2))
                return true;
            return matchAbbrev(team1, team2);
        }
  private:
    QString    m_team1;
    QString    m_team2;
    QString    m_abbrev1;
    QString    m_abbrev2;
    QString    m_textState;
    int        m_period    {0};
    bool       m_finished  {false};
};
//using GameStateList = QList<GameState>;

class RecExtDataSource;
class RecordingExtender;

class RecExtDataPage : public QObject
{
    Q_OBJECT;
  public:
    RecExtDataSource* getSource()
        {return qobject_cast<RecExtDataSource*>(parent()); }
    /// Get the current time. Overridden by the testing code.
    virtual QDateTime getNow() {return MythDate::current(); }
    virtual bool timeIsClose(const QDateTime& eventStart);
    virtual bool findGameInfo(ActiveGame& game) = 0;
    virtual GameState findGameScore(ActiveGame& game) = 0;
    static QJsonObject walkJsonPath(QJsonObject& object, const QStringList& path);
    /// Get the JSON document associated with this object.
    /// @returns a JSonDocument object.
    QJsonDocument getDoc() { return m_doc; }
    static bool getJsonInt   (const QJsonObject& object, QStringList& path,  int& value);
    static bool getJsonInt   (const QJsonObject& object, const QString& key, int& value);
    static bool getJsonString(const QJsonObject& object, QStringList& path,  QString& value);
    static bool getJsonString(const QJsonObject& object, const QString& key, QString& value);
    static bool getJsonObject(const QJsonObject& object, QStringList& path,  QJsonObject& value);
    static bool getJsonObject(const QJsonObject& object, const QString& key, QJsonObject& value);
    static bool getJsonArray (const QJsonObject& object, const QString& key, QJsonArray& value);

    /// Create a new RecExtDataPage object.
    /// @param doc The JSON document to associate with this object.
    RecExtDataPage(QObject* parent, QJsonDocument doc) :
        QObject(parent), m_doc(std::move(doc)) {};
  protected:
    QJsonDocument m_doc;
};

class RecExtEspnDataPage : public RecExtDataPage
{
    // Games status codes.
    enum GameStatus : std::uint8_t {
        TBD = 0,
        SCHEDULED = 1,
        INPROGRESS = 2,
        FINAL = 3,
        FORFEIT = 4,
        CANCELLED = 5,
        POSTPONED = 6,
        DELAYED = 7,
        SUSPENDED = 8,
        FORFEIT_HOME_TEAM = 9,
        FORFEIT_AWAY_TEAM = 10,
        RAIN_DELAY = 17,
        BEGINNING_OF_PERIOD = 21,
        END_OF_PERIOD = 22,
        HALFTIME = 23,
        OVERTIME = 24,
        FIRST_HALF = 25,
        SECOND_HALF = 26,
        ABANDONED = 27,
        FULL_TIME = 28,
        RESCHEDULED = 29,
        START_LIST = 30,
        INTERMEDIATE = 31,
        UNOFFICIAL = 32,
        MEDAL_OFFICIAL = 33,
        GROUPINGS_OFFICIAL = 34,
        PLAY_COMPLETE = 35,
        OFFICIAL_EVENT_SHORTENED = 36,
        CORRECTED_RESULT = 37,
        RETIRED = 38,
        BYE = 39,
        WALKOVER = 40,
        ESPNVOID = 41, // VOID makes win32 die horribly
        PRELIMINARY = 42,
        GOLDEN_TIME = 43,
        SHOOTOUT = 44,
        FINAL_SCORE_AFTER_EXTRA_TIME = 45,
        FINAL_SCORE_AFTER_GOLDEN_GOAL = 46,
        FINAL_SCORE_AFTER_PENALTIES = 47,
        END_EXTRA_TIME = 48,
        EXTRA_TIME_HALF_TIME = 49,
        FIXTURE_NO_LIVE_COVERAGE = 50,
        FINAL_SCORE_ABANDONED = 51,
    };
    static const QList<GameStatus> kFinalStatuses;

  public:
    RecExtEspnDataPage(QObject* parent, QJsonDocument doc) :
        RecExtDataPage(parent, std::move(doc)) {};
    bool findGameInfo(ActiveGame& game) override;
    GameState findGameScore(ActiveGame& game) override;
};

class RecExtMlbDataPage : public RecExtDataPage
{
  public:
    RecExtMlbDataPage(QObject* parent, QJsonDocument doc) :
        RecExtDataPage(parent, std::move(doc)) {};
    bool findGameInfo(ActiveGame& game) override;
    GameState findGameScore(ActiveGame& game) override;
    bool parseGameObject(const QJsonObject& gameObject, ActiveGame& game);
};

class RecExtDataSource : public QObject
{
    Q_OBJECT;

  public:
    RecordingExtender* getExtender()
        { return qobject_cast<RecordingExtender*>(parent()); }
    virtual RecExtDataPage* newPage(const QJsonDocument& doc) = 0;
    static void clearCache();
    virtual QUrl makeInfoUrl(const SportInfo& info, const QDateTime& dt) = 0;
    virtual QUrl makeGameUrl(const ActiveGame& game, const QString& str) = 0;
    virtual QUrl findInfoUrl(ActiveGame& game, SportInfo& info) = 0;
    virtual RecExtDataPage* loadPage(const ActiveGame& game, const QUrl& _url) = 0;

  protected:
    explicit RecExtDataSource(QObject *parent) : QObject(parent) {};

    QUrl m_url;
    static QHash<QString,QJsonDocument> s_downloadedJson;
};

class RecExtEspnDataSource : public RecExtDataSource
{
  public:
    explicit RecExtEspnDataSource(QObject *parent) : RecExtDataSource(parent) {};
    RecExtDataPage* newPage(const QJsonDocument& doc) override
        { return new RecExtEspnDataPage(this, doc); }
    QUrl makeInfoUrl(const SportInfo& info, const QDateTime& dt) override;
    QUrl makeGameUrl(const ActiveGame& game, const QString& str) override;
    QUrl findInfoUrl(ActiveGame& game, SportInfo& info) override;
    RecExtDataPage* loadPage(const ActiveGame& game, const QUrl& _url) override;
};

class RecExtMlbDataSource : public RecExtDataSource
{
  public:
    explicit RecExtMlbDataSource(QObject *parent) : RecExtDataSource(parent) {}
    RecExtDataPage* newPage(const QJsonDocument& doc) override
        { return new RecExtMlbDataPage(this, doc); }
    QUrl makeInfoUrl(const SportInfo& info, const QDateTime& dt) override;
    QUrl makeGameUrl(const ActiveGame& game, const QString& str) override;
    QUrl findInfoUrl(ActiveGame& game, SportInfo& info) override;
    RecExtDataPage* loadPage(const ActiveGame& game, const QUrl& _url) override;
};

//
//
//
class RecordingExtender : public QObject, public MThread
{
    Q_OBJECT;
    friend class TestRecordingExtender;

  public:
    static void create(Scheduler *scheduler, RecordingInfo& ri);
    void run(void) override; // MThread
    ~RecordingExtender() override;
    static void nameCleanup(const SportInfo& info, QString& name1, QString& name2);

  private:
    RecordingExtender () : MThread("RecordingExtender") {}

    void addNewRecording(int recordedID);

    // Get provider information from database.
    bool findKnownSport(const QString& _title,
                        AutoExtendType type,
                        SportInfoList& info) const;
    static void clearDownloadedInfo();

    // Process recordings
    virtual RecExtDataSource* createDataSource(AutoExtendType type);
    static bool parseProgramInfo(const QString& subtitle, const QString& description,
                                 QString& team1, QString& team2);
    static QString ruleIdAsString(const RecordingRule *rr);
    static void finishRecording(const RecordingInfo* ri, RecordingRule *rr, const ActiveGame& game);
    void extendRecording(const RecordingInfo* ri, RecordingRule *rr, const ActiveGame& game);
    static void unchangedRecording(const RecordingInfo* ri, RecordingRule *rr, const ActiveGame& game);
    void processNewRecordings();
    void processActiveRecordings();
    void checkDone();

    // Cleanup
    static void nameCleanup(const SportInfo& info, QString& name);
    void expireOverrides();

    /// The single instance of a RecordingExtender.
    static RecordingExtender *s_singleton;
    /// Interlock the scheduler thread crating this process, and this
    /// process determining whether it should continue running.
    static QMutex             s_createLock;
    /// Whether the RecordingExtender process is running.
    bool               m_running           {true};
    /// Pointer to the scheduler. Needed to retrieve the scheduler's
    /// view of ongoing recordings.
    Scheduler         *m_scheduler         {nullptr};
    /// New recordings are added by the scheduler process and removed
    /// by this process.
    QMutex             m_newRecordingsLock;
    /// Newly started recordings to process.
    QList<int>         m_newRecordings;
    /// Currently ongoing games to track.
    QList<ActiveGame>  m_activeGames;
    /// Recordings that have had an override rule creates.
    QList<int>         m_overrideRules;

    /// Testing data
    uint               m_forcedYearforTesting {0};
};

#endif // RECORDING_EXTENDER_H_
