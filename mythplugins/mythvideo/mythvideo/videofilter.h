#ifndef VIDEOFILTER_H_
#define VIDEOFILTER_H_

/*
    videofilter.h

    (c) 2003 Xavier Hervy
    Part of the mythTV project

    Class to let user filter the video list

*/

#include <mythtv/mythdialogs.h>

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
        kFilterParentalLevelChanged = (1 << 8)
    };

  public:
    VideoFilterSettings(bool loaddefaultsettings = true,
                        const QString &_prefix = "");
    VideoFilterSettings(const VideoFilterSettings &rhs);
    VideoFilterSettings &operator=(const VideoFilterSettings &rhs);

    bool matches_filter(const Metadata &mdata) const;
    bool meta_less_than(const Metadata &lhs, const Metadata &rhs) const;

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

    int getParentalLevel() const { return m_parental_level; }
    void setParentalLevel(int parental_level)
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
    int year;
    int runtime;
    int userrating;
    int browse;
    int m_inetref;
    int m_coverfile;
    ordering orderby;
    int m_parental_level;
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

class VideoFilterDialog : public MythThemedDialog
{

  Q_OBJECT

    //
    //  Dialog to manipulate the data
    //

  public:
    VideoFilterDialog(FilterSettingsProxy *fsp,
                       MythMainWindow *parent_,
                       QString window_name,
                       QString theme_filename,
                       const VideoList &video_list,
                       const char *name_ = 0);
    ~VideoFilterDialog();

    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();

  public slots:

    void takeFocusAwayFromEditor(bool up_or_down);
    void saveAndExit();
    void saveAsDefault();
    void setYear(int new_year);
    void setUserRating(int new_userrating);
    void setCategory(int new_category);
    void setCountry(int new_country);
    void setGenre(int new_genre);
    void setRunTime(int new_runtime);
    void setBrowse(int new_browse);
    void setInetRef(int new_inetref);
    void setCoverFile(int new_coverfile);
    void setOrderby(int new_orderby);

 private:
    void update_numvideo();
    VideoFilterSettings m_settings;
    //
    //  GUI Stuff
    //
    UISelectorType      *browse_select;
    UISelectorType      *orderby_select;
    UISelectorType      *year_select;
    UISelectorType  *userrating_select;
    UISelectorType  *category_select;
    UISelectorType  *country_select;
    UISelectorType  *genre_select;
    UISelectorType  *runtime_select;
    UITextButtonType    *save_button;
    UITextButtonType    *done_button;
    UITextType          *numvideos_text;

    UISelectorType  *m_intetref_select;
    UISelectorType  *m_coverfile_select;

    FilterSettingsProxy *m_fsp;
    const VideoList &m_video_list;
};

#endif
