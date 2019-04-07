#ifndef VIDEOFILTER_H_
#define VIDEOFILTER_H_

// MythTV headers
#include "mythscreentype.h"
#include "parentalcontrols.h"

// Qt headers
#include <QCoreApplication>
#include <QDate>

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIButton;
class MythUIText;

class VideoMetadata;
class VideoList;

enum GenreFilter {
    kGenreFilterAll = -1,
    kGenreFilterUnknown = 0
};

enum CountryFilter {
    kCountryFilterAll = -1,
    kCountryFilterUnknown = 0
};

enum CastFilter {
    kCastFilterAll = -1,
    kCastFilterUnknown = 0
};

enum CategoryFilter {
    kCategoryFilterAll = -1,
    kCategoryFilterUnknown = 0
};

enum YearFilter {
    kYearFilterAll = -1,
    kYearFilterUnknown = 0
};

enum RuntimeFilter {
    kRuntimeFilterAll = -2,
    kRuntimeFilterUnknown = -1
};

enum UserRatingFilter {
    kUserRatingFilterAll = -1
};

enum BrowseFilter {
    kBrowseFilterAll = -1
};

enum WatchedFilter {
    kWatchedFilterAll = -1
};

enum InetRefFilter {
    kInetRefFilterAll = -1,
    kInetRefFilterUnknown = 0
};

enum CoverFileFilter {
    kCoverFileFilterAll = -1,
    kCoverFileFilterNone = 0
};

class VideoFilterSettings
{
     Q_DECLARE_TR_FUNCTIONS(VideoFilterSettings);

  public:
    static const unsigned int FILTER_MASK = 0xFFFE;
    static const unsigned int SORT_MASK = 0x1;
    enum FilterChanges {
        kSortOrderChanged = (1 << 0),
        kFilterCategoryChanged = (1 << 1),
        kFilterGenreChanged = (1 << 2),
        kFilterCountryChanged = (1 << 3),
        kFilterYearChanged = (1 << 4),
        kFilterRuntimeChanged = (1 << 5),
        kFilterUserRatingChanged = (1 << 6),
        kFilterBrowseChanged = (1 << 7),
        kFilterInetRefChanged = (1 << 8),
        kFilterCoverFileChanged = (1 << 9),
        kFilterParentalLevelChanged = (1 << 10),
        kFilterCastChanged = (1 << 11),
        kFilterWatchedChanged = (1 << 12),
        kFilterTextFilterChanged = (1 << 13)
    };

  public:
    VideoFilterSettings(bool loaddefaultsettings = true,
                        const QString &_prefix = "");
    VideoFilterSettings(const VideoFilterSettings &rhs);
    VideoFilterSettings &operator=(const VideoFilterSettings &rhs);

    bool matches_filter(const VideoMetadata &mdata) const;
    bool meta_less_than(const VideoMetadata &lhs, const VideoMetadata &rhs) const;

    void saveAsDefault();

    enum ordering
    {
        // These values must be explicitly assigned; they represent
        // database values
        kOrderByTitle = 0,
        kOrderByYearDescending = 1,
        kOrderByUserRatingDescending = 2,
        kOrderByLength = 3,
        kOrderByFilename = 4,
        kOrderByID = 5,
        kOrderBySeasonEp = 6,
        kOrderByDateAddedDescending = 7
    };

    int GetCategory() const { return m_category; }
    void SetCategory(int lcategory)
    {
        m_changed_state |= kFilterCategoryChanged;
        m_category = lcategory;
    }

    int getGenre() const { return m_genre; }
    void setGenre(int lgenre)
    {
        m_changed_state |= kFilterGenreChanged;
        m_genre = lgenre;
    }

    int GetCast() const { return m_cast; }
    void SetCast(int lcast)
    {
        m_changed_state |= kFilterCastChanged;
        m_cast = lcast;
    }

    int getCountry() const { return m_country; }
    void setCountry(int lcountry)
    {
        m_changed_state |= kFilterCountryChanged;
        m_country = lcountry;
    }

    int getYear() const { return m_year; }
    void SetYear(int lyear)
    {
        m_changed_state |= kFilterYearChanged;
        m_year = lyear;
    }

    int getRuntime() const { return m_runtime; }
    void setRuntime(int lruntime)
    {
        m_changed_state |= kFilterRuntimeChanged;
        m_runtime = lruntime;
    }

    int GetUserRating() const { return m_userrating; }
    void SetUserRating(int luserrating)
    {
        m_changed_state |= kFilterUserRatingChanged;
        m_userrating = luserrating;
    }

    int GetBrowse() const {return m_browse; }
    void SetBrowse(int lbrowse)
    {
        m_changed_state |= kFilterBrowseChanged;
        m_browse = lbrowse;
    }

    int GetWatched() const {return m_watched; }
    void SetWatched(int lwatched)
    {
        m_changed_state |= kFilterWatchedChanged;
        m_watched = lwatched;
    }

    ordering getOrderby() const { return m_orderby; }
    void setOrderby(ordering lorderby)
    {
        m_changed_state |= kSortOrderChanged;
        m_orderby = lorderby;
    }

    QString getTextFilter() const { return m_textfilter; }
    void setTextFilter(const QString& val);

    ParentalLevel::Level getParentalLevel() const { return m_parental_level; }
    void setParentalLevel(ParentalLevel::Level parental_level)
    {
        m_changed_state |= kFilterParentalLevelChanged;
        m_parental_level = parental_level;
    }

    int getInteRef() const { return m_inetref; }
    void SetInetRef(int inetref)
    {
        m_inetref = inetref;
        m_changed_state |= kFilterInetRefChanged;
    }

    int GetCoverFile() const { return m_coverfile; }
    void SetCoverFile(int coverfile)
    {
        m_coverfile = coverfile;
        m_changed_state |= kFilterCoverFileChanged;
    }

    unsigned int getChangedState()
    {
        unsigned int ret = m_changed_state;
        m_changed_state = 0;
        return ret;
    }

  private:
    int                  m_category       {kCategoryFilterAll};
    int                  m_genre          {kGenreFilterAll};
    int                  m_country        {kCountryFilterAll};
    int                  m_cast           {kCastFilterAll};
    int                  m_year           {kYearFilterAll};
    int                  m_runtime        {kRuntimeFilterAll};
    int                  m_userrating     {kUserRatingFilterAll};
    int                  m_browse         {kBrowseFilterAll};
    int                  m_watched        {kWatchedFilterAll};
    int                  m_inetref        {kInetRefFilterAll};
    int                  m_coverfile      {kCoverFileFilterAll};
    ordering             m_orderby        {kOrderByTitle};
    ParentalLevel::Level m_parental_level {ParentalLevel::plNone};
    QString              m_prefix;
    QString              m_textfilter;
    int                  m_season         {-1};
    int                  m_episode        {-1};
    QDate                m_insertdate;
    const QRegExp        m_re_season      {"(\\d+)[xX](\\d*)"};
    const QRegExp        m_re_date        {"-(\\d+)([dmw])"};

    unsigned int         m_changed_state  {0};
};

struct FilterSettingsProxy
{
    virtual ~FilterSettingsProxy() = default;
    virtual const VideoFilterSettings &getSettings() = 0;
    virtual void setSettings(const VideoFilterSettings &settings) = 0;
};

template <typename T>
class BasicFilterSettingsProxy : public FilterSettingsProxy
{
  public:
    explicit BasicFilterSettingsProxy(T &type) : m_type(type) {}

    const VideoFilterSettings &getSettings() override // FilterSettingsProxy
    {
        return m_type.getCurrentVideoFilter();
    }

    void setSettings(const VideoFilterSettings &settings) override // FilterSettingsProxy
    {
        m_type.setCurrentVideoFilter(settings);
    }

  private:
    T &m_type;
};

class VideoFilterDialog : public MythScreenType
{

  Q_OBJECT

  public:
    VideoFilterDialog( MythScreenStack *lparent, const QString& lname,
                       VideoList *video_list);
    ~VideoFilterDialog();

    bool Create() override; // MythScreenType

  signals:
    void filterChanged();

  public slots:
    void saveAndExit();
    void saveAsDefault();
    void SetYear(MythUIButtonListItem *item);
    void SetUserRating(MythUIButtonListItem *item);
    void SetCategory(MythUIButtonListItem *item);
    void setCountry(MythUIButtonListItem *item);
    void setGenre(MythUIButtonListItem *item);
    void SetCast(MythUIButtonListItem *item);
    void setRunTime(MythUIButtonListItem *item);
    void SetBrowse(MythUIButtonListItem *item);
    void SetWatched(MythUIButtonListItem *item);
    void SetInetRef(MythUIButtonListItem *item);
    void SetCoverFile(MythUIButtonListItem *item);
    void setOrderby(MythUIButtonListItem *item);
    void setTextFilter();

 private:
    void fillWidgets();
    void update_numvideo();
    VideoFilterSettings m_settings;

    MythUIButtonList  *m_browseList     {nullptr};
    MythUIButtonList  *m_watchedList    {nullptr};
    MythUIButtonList  *m_orderbyList    {nullptr};
    MythUIButtonList  *m_yearList       {nullptr};
    MythUIButtonList  *m_userratingList {nullptr};
    MythUIButtonList  *m_categoryList   {nullptr};
    MythUIButtonList  *m_countryList    {nullptr};
    MythUIButtonList  *m_genreList      {nullptr};
    MythUIButtonList  *m_castList       {nullptr};
    MythUIButtonList  *m_runtimeList    {nullptr};
    MythUIButtonList  *m_inetrefList    {nullptr};
    MythUIButtonList  *m_coverfileList  {nullptr};
    MythUIButton      *m_saveButton     {nullptr};
    MythUIButton      *m_doneButton     {nullptr};
    MythUIText        *m_numvideosText  {nullptr};
    MythUITextEdit    *m_textfilter     {nullptr};

    const VideoList &m_videoList;
    FilterSettingsProxy *m_fsp          {nullptr};
};

#endif
