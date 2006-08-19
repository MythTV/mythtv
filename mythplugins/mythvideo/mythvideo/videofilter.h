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
    static const unsigned int SORT_ORDER_CHANGED = (1 << 0);
    static const unsigned int FILTER_CATEGORY_CHANGED = (1 << 1);
    static const unsigned int FILTER_GENRE_CHANGED = (1 << 2);
    static const unsigned int FILTER_COUNTRY_CHANGED = (1 << 3);
    static const unsigned int FILTER_YEAR_CHANGED = (1 << 4);
    static const unsigned int FILTER_RUNTIME_CHANGED = (1 << 5);
    static const unsigned int FILTER_USERRATING_CHANGED = (1 << 6);
    static const unsigned int FILTER_BROWSE_CHANGED = (1 << 7);
    static const unsigned int FILTER_PARENTAL_LEVEL_CHANGED = (1 << 8);

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
        m_changed_state |= FILTER_CATEGORY_CHANGED;
        category = lcategory;
    }

    int getGenre() const { return genre; }
    void setGenre(int lgenre)
    {
        m_changed_state |= FILTER_GENRE_CHANGED;
        genre = lgenre;
    }

    int getCountry() const { return country; }
    void setCountry(int lcountry)
    {
        m_changed_state |= FILTER_COUNTRY_CHANGED;
        country = lcountry;
    }

    int getYear() const { return year; }
    void setYear(int lyear)
    {
        m_changed_state |= FILTER_YEAR_CHANGED;
        year = lyear;
    }

    int getRuntime() const { return runtime; }
    void setRuntime(int lruntime)
    {
        m_changed_state |= FILTER_RUNTIME_CHANGED;
        runtime = lruntime;
    }

    int getUserrating() const { return userrating; }
    void setUserrating(int luserrating)
    {
        m_changed_state |= FILTER_USERRATING_CHANGED;
        userrating = luserrating;
    }

    int getBrowse() const {return browse; }
    void setBrowse(int lbrowse)
    {
        m_changed_state |= FILTER_BROWSE_CHANGED;
        browse = lbrowse;
    }

    ordering getOrderby() const { return orderby; }
    void setOrderby(ordering lorderby)
    {
        m_changed_state |= SORT_ORDER_CHANGED;
        orderby = lorderby;
    }

    int getParentalLevel() const { return m_parental_level; }
    void setParentalLevel(int parental_level)
    {
        m_changed_state |= FILTER_PARENTAL_LEVEL_CHANGED;
        m_parental_level = parental_level;
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
    void setOrderby(/* VideoFilterSettings::ordering */ int new_orderby);

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

    FilterSettingsProxy *m_fsp;
    const VideoList &m_video_list;
};

#endif
