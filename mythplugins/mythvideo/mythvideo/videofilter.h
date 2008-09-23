#ifndef VIDEOFILTER_H_
#define VIDEOFILTER_H_

// Mythui headers
#include "mythtv/libmythui/mythscreentype.h"
#include "mythtv/libmythui/mythlistbutton.h"
#include "mythtv/libmythui/mythuibutton.h"
#include "mythtv/libmythui/mythuitext.h"

// Mythvideo headers
#include "parentalcontrols.h"

class Metadata;
class VideoList;

class VideoFilterSettings
{
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
        kFilterCastChanged = (1 << 11)
    };

  public:
    VideoFilterSettings(bool loaddefaultsettings = true,
                        const QString &_prefix = "");
    VideoFilterSettings(const VideoFilterSettings &rhs);
    VideoFilterSettings &operator=(const VideoFilterSettings &rhs);

    bool matches_filter(const Metadata &mdata) const;
    bool meta_less_than(const Metadata &lhs, const Metadata &rhs,
                        bool sort_ignores_case) const;

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
        kOrderByID = 5
    };

    int getCategory() const { return category; }
    void setCategory(int lcategory)
    {
        m_changed_state |= kFilterCategoryChanged;
        category = lcategory;
    }

    int getGenre() const { return genre; }
    void setGenre(int lgenre)
    {
        m_changed_state |= kFilterGenreChanged;
        genre = lgenre;
    }

    int getCast() const { return cast; }
    void setCast(int lcast)
    {
        m_changed_state |= kFilterCastChanged;
        cast = lcast;
    }

    int getCountry() const { return country; }
    void setCountry(int lcountry)
    {
        m_changed_state |= kFilterCountryChanged;
        country = lcountry;
    }

    int getYear() const { return year; }
    void setYear(int lyear)
    {
        m_changed_state |= kFilterYearChanged;
        year = lyear;
    }

    int getRuntime() const { return runtime; }
    void setRuntime(int lruntime)
    {
        m_changed_state |= kFilterRuntimeChanged;
        runtime = lruntime;
    }

    int getUserrating() const { return userrating; }
    void setUserrating(int luserrating)
    {
        m_changed_state |= kFilterUserRatingChanged;
        userrating = luserrating;
    }

    int getBrowse() const {return browse; }
    void setBrowse(int lbrowse)
    {
        m_changed_state |= kFilterBrowseChanged;
        browse = lbrowse;
    }

    ordering getOrderby() const { return orderby; }
    void setOrderby(ordering lorderby)
    {
        m_changed_state |= kSortOrderChanged;
        orderby = lorderby;
    }

    ParentalLevel::Level getParentalLevel() const { return m_parental_level; }
    void setParentalLevel(ParentalLevel::Level parental_level)
    {
        m_changed_state |= kFilterParentalLevelChanged;
        m_parental_level = parental_level;
    }

    int getInteRef() const { return m_inetref; }
    void setInetRef(int inetref)
    {
        m_inetref = inetref;
        m_changed_state |= kFilterInetRefChanged;
    }

    int getCoverFile() const { return m_coverfile; }
    void setCoverFile(int coverfile)
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
    int category;
    int genre;
    int country;
    int cast;
    int year;
    int runtime;
    int userrating;
    int browse;
    int m_inetref;
    int m_coverfile;
    ordering orderby;
    ParentalLevel::Level m_parental_level;
    QString prefix;

    unsigned int m_changed_state;
};

struct FilterSettingsProxy
{
    virtual ~FilterSettingsProxy() {}
    virtual const VideoFilterSettings &getSettings() = 0;
    virtual void setSettings(const VideoFilterSettings &settings) = 0;
};

template <typename T>
class BasicFilterSettingsProxy : public FilterSettingsProxy
{
  public:
    BasicFilterSettingsProxy(T &type) : m_type(type) {}

    const VideoFilterSettings &getSettings()
    {
        return m_type.getCurrentVideoFilter();
    }

    void setSettings(const VideoFilterSettings &settings)
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
    VideoFilterDialog( MythScreenStack *parent, QString name,
                       VideoList *video_list);
    ~VideoFilterDialog();

    bool Create(void);

  signals:
    void filterChanged(void);

  public slots:
    void saveAndExit(void);
    void saveAsDefault(void);
    void setYear(MythListButtonItem *item);
    void setUserRating(MythListButtonItem *item);
    void setCategory(MythListButtonItem *item);
    void setCountry(MythListButtonItem *item);
    void setGenre(MythListButtonItem *item);
    void setCast(MythListButtonItem *item);
    void setRunTime(MythListButtonItem *item);
    void setBrowse(MythListButtonItem *item);
    void setInetRef(MythListButtonItem *item);
    void setCoverFile(MythListButtonItem *item);
    void setOrderby(MythListButtonItem *item);

 private:
    void fillWidgets(void);
    void update_numvideo(void);
    VideoFilterSettings m_settings;

    MythListButton  *m_browseList;
    MythListButton  *m_orderbyList;
    MythListButton  *m_yearList;
    MythListButton  *m_userratingList;
    MythListButton  *m_categoryList;
    MythListButton  *m_countryList;
    MythListButton  *m_genreList;
    MythListButton  *m_castList;
    MythListButton  *m_runtimeList;
    MythListButton  *m_inetrefList;
    MythListButton  *m_coverfileList;
    MythUIButton    *m_saveButton;
    MythUIButton    *m_doneButton;
    MythUIText      *m_numvideosText;

    const VideoList &m_videoList;
    FilterSettingsProxy *m_fsp;
};

#endif
