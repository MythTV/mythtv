#include <set>

#include "mythcontext.h"

#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "globals.h"
#include "dbaccess.h"
#include "videometadatalistmanager.h"
#include "videoutils.h"
#include "mythdate.h"

#include "videolist.h"
#include "videofilter.h"

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

VideoFilterSettings::VideoFilterSettings(bool loaddefaultsettings,
                                         const QString& _prefix) :
    category(kCategoryFilterAll), genre(kGenreFilterAll),
    country(kCountryFilterAll), cast(kCastFilterAll),
    year(kYearFilterAll), runtime(kRuntimeFilterAll),
    userrating(kUserRatingFilterAll), browse(kBrowseFilterAll),
    watched(kWatchedFilterAll), m_inetref(kInetRefFilterAll),
    m_coverfile(kCoverFileFilterAll), orderby(kOrderByTitle),
    m_parental_level(ParentalLevel::plNone), textfilter(""),
    season(-1), episode(-1), insertdate(QDate()),
    re_season("(\\d+)[xX](\\d*)"), re_date("-(\\d+)([dmw])"),
    m_changed_state(0)
{
    if (_prefix.isEmpty())
        prefix = "VideoDefault";
    else
        prefix = _prefix + "Default";

    // do nothing yet
    if (loaddefaultsettings)
    {
        category = gCoreContext->GetNumSetting(QString("%1Category").arg(prefix),
                                           kCategoryFilterAll);
        genre = gCoreContext->GetNumSetting(QString("%1Genre").arg(prefix),
                                        kGenreFilterAll);
        country = gCoreContext->GetNumSetting(QString("%1Country").arg(prefix),
                                          kCountryFilterAll);
        cast = gCoreContext->GetNumSetting(QString("%1Cast").arg(prefix),
                                        kCastFilterAll);
        year = gCoreContext->GetNumSetting(QString("%1Year").arg(prefix),
                                       kYearFilterAll);
        runtime = gCoreContext->GetNumSetting(QString("%1Runtime").arg(prefix),
                                          kRuntimeFilterAll);
        userrating =
                gCoreContext->GetNumSetting(QString("%1Userrating").arg(prefix),
                                        kUserRatingFilterAll);
        browse = gCoreContext->GetNumSetting(QString("%1Browse").arg(prefix),
                                         kBrowseFilterAll);
        watched = gCoreContext->GetNumSetting(QString("%1Watched").arg(prefix),
                                         kWatchedFilterAll);
        m_inetref = gCoreContext->GetNumSetting(QString("%1InetRef").arg(prefix),
                kInetRefFilterAll);
        m_coverfile = gCoreContext->GetNumSetting(QString("%1CoverFile")
                .arg(prefix), kCoverFileFilterAll);
        orderby = (ordering)gCoreContext->GetNumSetting(QString("%1Orderby")
                                                    .arg(prefix),
                                                    kOrderByTitle);
    }
}

VideoFilterSettings::VideoFilterSettings(const VideoFilterSettings &rhs) :
    m_changed_state(0)
{
    *this = rhs;
}

VideoFilterSettings &
VideoFilterSettings::operator=(const VideoFilterSettings &rhs)
{
    prefix = rhs.prefix;

    if (category != rhs.category)
    {
        m_changed_state |= kFilterCategoryChanged;
        category = rhs.category;
    }

    if (genre != rhs.genre)
    {
        m_changed_state |= kFilterGenreChanged;
        genre = rhs.genre;
    }

    if (country != rhs.country)
    {
        m_changed_state |= kFilterCountryChanged;
        country = rhs.country;
    }

    if (cast != rhs.cast)
    {
        m_changed_state |= kFilterCastChanged;
        cast = rhs.cast;
    }

    if (year != rhs.year)
    {
        m_changed_state |= kFilterYearChanged;
        year = rhs.year;
    }

    if (runtime != rhs.runtime)
    {
        m_changed_state |= kFilterRuntimeChanged;
        runtime = rhs.runtime;
    }

    if (userrating != rhs.userrating)
    {
        m_changed_state |= kFilterUserRatingChanged;
        userrating = rhs.userrating;
    }

    if (browse != rhs.browse)
    {
        m_changed_state |= kFilterBrowseChanged;
        browse = rhs.browse;
    }

    if (watched != rhs.watched)
    {
        m_changed_state |= kFilterWatchedChanged;
        watched = rhs.watched;
    }

    if (m_inetref != rhs.m_inetref)
    {
        m_changed_state |= kFilterInetRefChanged;
        m_inetref = rhs.m_inetref;
    }

    if (m_coverfile != rhs.m_coverfile)
    {
        m_changed_state |= kFilterCoverFileChanged;
        m_coverfile = rhs.m_coverfile;
    }

    if (orderby != rhs.orderby)
    {
        m_changed_state |= kSortOrderChanged;
        orderby = rhs.orderby;
    }

    if (m_parental_level != rhs.m_parental_level)
    {
        m_changed_state |= kFilterParentalLevelChanged;
        m_parental_level = rhs.m_parental_level;
    }

    if (textfilter != rhs.textfilter)
    {
        textfilter = rhs.textfilter;
        m_changed_state |= kFilterTextFilterChanged;
    }
    if (season != rhs.season || episode != rhs.episode)
    {
        season = rhs.season;
        episode = rhs.episode;
        m_changed_state |= kFilterTextFilterChanged;
    }
    if (insertdate != rhs.insertdate)
    {
        insertdate = rhs.insertdate;
        m_changed_state |= kFilterTextFilterChanged;
    }

    return *this;
}

void VideoFilterSettings::saveAsDefault()
{
    gCoreContext->SaveSetting(QString("%1Category").arg(prefix), category);
    gCoreContext->SaveSetting(QString("%1Genre").arg(prefix), genre);
    gCoreContext->SaveSetting(QString("%1Cast").arg(prefix), cast);
    gCoreContext->SaveSetting(QString("%1Country").arg(prefix), country);
    gCoreContext->SaveSetting(QString("%1Year").arg(prefix), year);
    gCoreContext->SaveSetting(QString("%1Runtime").arg(prefix), runtime);
    gCoreContext->SaveSetting(QString("%1Userrating").arg(prefix), userrating);
    gCoreContext->SaveSetting(QString("%1Browse").arg(prefix), browse);
    gCoreContext->SaveSetting(QString("%1Watched").arg(prefix), watched);
    gCoreContext->SaveSetting(QString("%1InetRef").arg(prefix), m_inetref);
    gCoreContext->SaveSetting(QString("%1CoverFile").arg(prefix), m_coverfile);
    gCoreContext->SaveSetting(QString("%1Orderby").arg(prefix), orderby);
    gCoreContext->SaveSetting(QString("%1Filter").arg(prefix), textfilter);
}

bool VideoFilterSettings::matches_filter(const VideoMetadata &mdata) const
{
    bool matches = true;

    //textfilter
    if (!textfilter.isEmpty())
    {
        matches = false;
        matches = (matches ||
                   mdata.GetTitle().contains(textfilter, Qt::CaseInsensitive));
        matches = (matches ||
                   mdata.GetSubtitle().contains(textfilter, Qt::CaseInsensitive));
        matches = (matches ||
                   mdata.GetPlot().contains(textfilter, Qt::CaseInsensitive));
    }
    //search for season with optionally episode nr.
    if (matches && (season != -1))
    {
        matches = (season == mdata.GetSeason());
        matches = (matches && (episode == -1 || episode == mdata.GetEpisode()));
    }
    if (matches && insertdate.isValid())
    {
        matches = (mdata.GetInsertdate().isValid() &&
                   mdata.GetInsertdate() >= insertdate);
    }
    if (matches && (genre != kGenreFilterAll))
    {
        matches = false;

        const VideoMetadata::genre_list &gl = mdata.GetGenres();
        for (VideoMetadata::genre_list::const_iterator p = gl.begin();
             p != gl.end(); ++p)
        {
            if ((matches = (p->first == genre)))
            {
                break;
            }
        }
    }

    if (matches && country != kCountryFilterAll)
    {
        matches = false;

        const VideoMetadata::country_list &cl = mdata.GetCountries();
        for (VideoMetadata::country_list::const_iterator p = cl.begin();
             p != cl.end(); ++p)
        {
            if ((matches = (p->first == country)))
            {
                break;
            }
        }
    }

    if (matches && cast != kCastFilterAll)
    {
        const VideoMetadata::cast_list &cl = mdata.GetCast();

        if ((cast == kCastFilterUnknown) && (cl.size() == 0))
        {
            matches = true;
        }
        else
        {
            matches = false;

            for (VideoMetadata::cast_list::const_iterator p = cl.begin();
                 p != cl.end(); ++p)
            {
                if ((matches = (p->first == cast)))
                {
                    break;
                }
            }
        }
    }

    if (matches && category != kCategoryFilterAll)
    {
        matches = (category == mdata.GetCategoryID());
    }

    if (matches && year != kYearFilterAll)
    {
        if (year == kYearFilterUnknown)
        {
            matches = ((mdata.GetYear() == 0) ||
                       (mdata.GetYear() == VIDEO_YEAR_DEFAULT));
        }
        else
        {
            matches = (year == mdata.GetYear());
        }
    }

    if (matches && runtime != kRuntimeFilterAll)
    {
        if (runtime == kRuntimeFilterUnknown)
        {
            matches = (mdata.GetLength() == 0);
        }
        else
        {
            matches = (runtime == (mdata.GetLength() / 30));
        }
    }

    if (matches && userrating != kUserRatingFilterAll)
    {
        matches = (mdata.GetUserRating() >= userrating);
    }

    if (matches && browse != kBrowseFilterAll)
    {
        matches = (mdata.GetBrowse() == browse);
    }

    if (matches && watched != kWatchedFilterAll)
    {
        matches = (mdata.GetWatched() == watched);
    }

    if (matches && m_inetref != kInetRefFilterAll)
    {
        matches = (mdata.GetInetRef() == VIDEO_INETREF_DEFAULT);
    }

    if (matches && m_coverfile != kCoverFileFilterAll)
    {
        matches = (IsDefaultCoverFile(mdata.GetCoverFile()));
    }

    if (matches && m_parental_level)
    {
        matches = ((mdata.GetShowLevel() != ParentalLevel::plNone) &&
                (mdata.GetShowLevel() <= m_parental_level));
    }

    return matches;
}

/// Compares two VideoMetadata instances
bool VideoFilterSettings::meta_less_than(const VideoMetadata &lhs,
                                         const VideoMetadata &rhs,
                                         bool sort_ignores_case) const
{
    bool ret = false;
    switch (orderby)
    {
        case kOrderByTitle:
        {
            VideoMetadata::SortKey lhs_key;
            VideoMetadata::SortKey rhs_key;
            if (lhs.HasSortKey() && rhs.HasSortKey())
            {
                lhs_key = lhs.GetSortKey();
                rhs_key = rhs.GetSortKey();
            }
            else
            {
                lhs_key = VideoMetadata::GenerateDefaultSortKey(lhs,
                                                           sort_ignores_case);
                rhs_key = VideoMetadata::GenerateDefaultSortKey(rhs,
                                                           sort_ignores_case);
            }
            ret = lhs_key < rhs_key;
            break;
        }
        case kOrderBySeasonEp:
        {
            if ((lhs.GetSeason() == rhs.GetSeason())
                && (lhs.GetEpisode() == rhs.GetEpisode())
                && (lhs.GetSeason() == 0)
                && (rhs.GetSeason() == 0)
                && (lhs.GetEpisode() == 0)
                && (rhs.GetEpisode() == 0))
            {
                VideoMetadata::SortKey lhs_key;
                VideoMetadata::SortKey rhs_key;
                if (lhs.HasSortKey() && rhs.HasSortKey())
                {
                    lhs_key = lhs.GetSortKey();
                    rhs_key = rhs.GetSortKey();
                }
                else
                {
                    lhs_key = VideoMetadata::GenerateDefaultSortKey(lhs,
                                                               sort_ignores_case);
                    rhs_key = VideoMetadata::GenerateDefaultSortKey(rhs,
                                                               sort_ignores_case);
                }
                ret = lhs_key < rhs_key;
            }
            else if ((lhs.GetSeason() == rhs.GetSeason())
                     && (lhs.GetTitle() == rhs.GetTitle()))
                ret = (lhs.GetEpisode() < rhs.GetEpisode());
            else
                ret = (lhs.GetSeason() < rhs.GetSeason());
            break;
        }
        case kOrderByYearDescending:
        {
            ret = (lhs.GetYear() > rhs.GetYear());
            break;
        }
        case kOrderByUserRatingDescending:
        {
            ret = (lhs.GetUserRating() > rhs.GetUserRating());
            break;
        }
        case kOrderByLength:
        {
            ret = (lhs.GetLength() < rhs.GetLength());
            break;
        }
        case kOrderByFilename:
        {
            QString lhsfn(sort_ignores_case ?
                          lhs.GetFilename().toLower() : lhs.GetFilename());
            QString rhsfn(sort_ignores_case ?
                          rhs.GetFilename().toLower() : rhs.GetFilename());
            ret = QString::localeAwareCompare(lhsfn, rhsfn) < 0;
            break;
        }
        case kOrderByID:
        {
            ret = (lhs.GetID() < rhs.GetID());
            break;
        }
        case kOrderByDateAddedDescending:
        {
            ret = (lhs.GetInsertdate() > rhs.GetInsertdate());
            break;
        }
        default:
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error: unknown sort type %1")
                    .arg(orderby));
        }
    }

    return ret;
}

void VideoFilterSettings::setTextFilter(QString val)
{
    m_changed_state |= kFilterTextFilterChanged;
    if (re_season.indexIn(val) != -1)
    {
        bool res;
        QStringList list = re_season.capturedTexts();
        season = list[1].toInt(&res);
        if (!res)
            season = -1;
        if (list.size() > 2) {
            episode = list[2].toInt(&res);
            if (!res)
                episode = -1;
        }
        else {
            episode = -1;
        }
        //clear \dX\d from string for string-search in plot/title/subtitle
        textfilter = val;
        textfilter.replace(re_season, "");
        textfilter = textfilter.simplified ();
    }
    else
    {
        textfilter = val;
        season = -1;
        episode = -1;
    }
    if (re_date.indexIn(textfilter) != -1)
    {
        QStringList list = re_date.capturedTexts();
        int modnr = list[1].toInt();
        QDate testdate = MythDate::current().date();
        switch(list[2].at(0).toLatin1())
        {
            case 'm': testdate = testdate.addMonths(-modnr);break;
            case 'd': testdate = testdate.addDays(-modnr);break;
            case 'w': testdate = testdate.addDays(-modnr * 7);break;
        }
        insertdate = testdate;
        textfilter.replace(re_date, "");
        textfilter = textfilter.simplified ();
    }
    else
    {
        //reset testdate
        insertdate = QDate();
    }
}

/////////////////////////////////
// VideoFilterDialog
/////////////////////////////////
VideoFilterDialog::VideoFilterDialog(MythScreenStack *lparent, QString lname,
        VideoList *video_list) : MythScreenType(lparent, lname),
    m_browseList(0), m_watchedList(0), m_orderbyList(0), m_yearList(0),
    m_userratingList(0), m_categoryList(0), m_countryList(0), m_genreList(0),
    m_castList(0), m_runtimeList(0), m_inetrefList(0), m_coverfileList(0),
    m_saveButton(0), m_doneButton(0), m_numvideosText(0), m_textfilter(0),
    m_videoList(*video_list)
{
    m_fsp = new BasicFilterSettingsProxy<VideoList>(*video_list);
    m_settings = m_fsp->getSettings();
}

VideoFilterDialog::~VideoFilterDialog()
{
    delete m_fsp;
}

bool VideoFilterDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "filter", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_textfilter, "textfilter_input", &err);
    UIUtilE::Assign(this, m_yearList, "year_select", &err);
    UIUtilE::Assign(this, m_userratingList, "userrating_select", &err);
    UIUtilE::Assign(this, m_categoryList, "category_select", &err);
    UIUtilE::Assign(this, m_countryList, "country_select", &err);
    UIUtilE::Assign(this, m_genreList, "genre_select", &err);
    UIUtilE::Assign(this, m_castList, "cast_select", &err);
    UIUtilE::Assign(this, m_runtimeList, "runtime_select", &err);
    UIUtilE::Assign(this, m_browseList, "browse_select", &err);
    UIUtilE::Assign(this, m_watchedList, "watched_select", &err);
    UIUtilE::Assign(this, m_inetrefList, "inetref_select", &err);
    UIUtilE::Assign(this, m_coverfileList, "coverfile_select", &err);
    UIUtilE::Assign(this, m_orderbyList, "orderby_select", &err);

    UIUtilE::Assign(this, m_doneButton, "done_button", &err);
    UIUtilE::Assign(this, m_saveButton, "save_button", &err);

    UIUtilE::Assign(this, m_numvideosText, "numvideos_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'filter'");
        return false;
    }

    BuildFocusList();

    fillWidgets();
    update_numvideo();

    connect(m_yearList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetYear(MythUIButtonListItem*)));
    connect(m_userratingList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetUserRating(MythUIButtonListItem*)));
    connect(m_categoryList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetCategory(MythUIButtonListItem*)));
    connect(m_countryList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setCountry(MythUIButtonListItem*)));
    connect(m_genreList,SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setGenre(MythUIButtonListItem*)));
    connect(m_castList,SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetCast(MythUIButtonListItem*)));
    connect(m_runtimeList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setRunTime(MythUIButtonListItem*)));
    connect(m_browseList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetBrowse(MythUIButtonListItem*)));
    connect(m_watchedList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetWatched(MythUIButtonListItem*)));
    connect(m_inetrefList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetInetRef(MythUIButtonListItem*)));
    connect(m_coverfileList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetCoverFile(MythUIButtonListItem*)));
    connect(m_orderbyList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setOrderby(MythUIButtonListItem*)));
    connect(m_textfilter, SIGNAL(valueChanged()),
            SLOT(setTextFilter()));

    connect(m_saveButton, SIGNAL(Clicked()), SLOT(saveAsDefault()));
    connect(m_doneButton, SIGNAL(Clicked()), SLOT(saveAndExit()));

    return true;
}

void VideoFilterDialog::update_numvideo()
{
    int video_count = m_videoList.TryFilter(m_settings);

    if (video_count > 0)
    {
        m_numvideosText->SetText(tr("Result of this filter : %n video(s)", "",
                                    video_count));
    }
    else
    {
        m_numvideosText->SetText(tr("Result of this filter : No Videos"));
    }
}

void VideoFilterDialog::fillWidgets()
{
    bool have_unknown_year = false;
    bool have_unknown_runtime = false;

    typedef std::set<int> int_list;
    int_list years;
    int_list runtimes;
    int_list user_ratings;

    const VideoMetadataListManager::metadata_list &mdl =
            m_videoList.getListCache().getList();
    for (VideoMetadataListManager::metadata_list::const_iterator p = mdl.begin();
         p != mdl.end(); ++p)
    {
        int year = (*p)->GetYear();
        if ((year == 0) || (year == VIDEO_YEAR_DEFAULT))
            have_unknown_year = true;
        else
            years.insert(year);

        int runtime = (*p)->GetLength();
        if (runtime == 0)
            have_unknown_runtime = true;
        else
            runtimes.insert(runtime / 30);

        user_ratings.insert(static_cast<int>((*p)->GetUserRating()));
    }

    // Category
    new MythUIButtonListItem(m_categoryList, tr("All", "Category"),
                           kCategoryFilterAll);

    const VideoCategory::entry_list &vcl =
            VideoCategory::GetCategory().getList();
    for (VideoCategory::entry_list::const_iterator p = vcl.begin();
            p != vcl.end(); ++p)
    {
        new MythUIButtonListItem(m_categoryList, p->second, p->first);
    }

    new MythUIButtonListItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN,
                           kCategoryFilterUnknown);
    m_categoryList->SetValueByData(m_settings.GetCategory());

    // Genre
    new MythUIButtonListItem(m_genreList, tr("All", "Genre"), kGenreFilterAll);

    const VideoGenre::entry_list &gl = VideoGenre::getGenre().getList();
    for (VideoGenre::entry_list::const_iterator p = gl.begin();
            p != gl.end(); ++p)
    {
        new MythUIButtonListItem(m_genreList, p->second, p->first);
    }

    new MythUIButtonListItem(m_genreList, VIDEO_GENRE_UNKNOWN, kGenreFilterUnknown);
    m_genreList->SetValueByData(m_settings.getGenre());

    // Cast
    new MythUIButtonListItem(m_castList, tr("All", "Cast"), kCastFilterAll);

    const VideoCast::entry_list &cl = VideoCast::GetCast().getList();
    for (VideoCast::entry_list::const_iterator p = cl.begin();
            p != cl.end(); ++p)
    {
        new MythUIButtonListItem(m_castList, p->second, p->first);
    }

    new MythUIButtonListItem(m_castList, VIDEO_CAST_UNKNOWN, kCastFilterUnknown);
    m_castList->SetValueByData(m_settings.GetCast());

    // Country
    new MythUIButtonListItem(m_countryList, tr("All", "Country"), kCountryFilterAll);

    const VideoCountry::entry_list &cnl = VideoCountry::getCountry().getList();
    for (VideoCountry::entry_list::const_iterator p = cnl.begin();
            p != cnl.end(); ++p)
    {
        new MythUIButtonListItem(m_countryList, p->second, p->first);
    }

    new MythUIButtonListItem(m_countryList, VIDEO_COUNTRY_UNKNOWN,
                           kCountryFilterUnknown);
    m_countryList->SetValueByData(m_settings.getCountry());

    // Year
    new MythUIButtonListItem(m_yearList, tr("All", "Year"), kYearFilterAll);

    for (int_list::const_reverse_iterator p = years.rbegin();
            p != years.rend(); ++p)
    {
        new MythUIButtonListItem(m_yearList, QString::number(*p), *p);
    }

    if (have_unknown_year)
        new MythUIButtonListItem(m_yearList, VIDEO_YEAR_UNKNOWN,
                               kYearFilterUnknown);

    m_yearList->SetValueByData(m_settings.getYear());

    // Runtime
    new MythUIButtonListItem(m_runtimeList, tr("All", "Runtime"), kRuntimeFilterAll);

    if (have_unknown_runtime)
        new MythUIButtonListItem(m_runtimeList, VIDEO_RUNTIME_UNKNOWN,
                               kRuntimeFilterUnknown);

    for (int_list::const_iterator p = runtimes.begin();
            p != runtimes.end(); ++p)
    {
        QString s = QString("%1 %2 ~ %3 %4").arg(*p * 30).arg(tr("minutes"))
                .arg((*p + 1) * 30).arg(tr("minutes"));
        new MythUIButtonListItem(m_runtimeList, s, *p);
    }

    m_runtimeList->SetValueByData(m_settings.getRuntime());

    // User Rating
    new MythUIButtonListItem(m_userratingList, tr("All", "User rating"),
                           kUserRatingFilterAll);

    for (int_list::const_reverse_iterator p = user_ratings.rbegin();
            p != user_ratings.rend(); ++p)
    {
        new MythUIButtonListItem(m_userratingList,
                               QString(">= %1").arg(QString::number(*p)),
                               *p);
    }

    m_userratingList->SetValueByData(m_settings.GetUserRating());

    // Browsable
    new MythUIButtonListItem(m_browseList, tr("All", "Browsable"),
                             kBrowseFilterAll);
    new MythUIButtonListItem(m_browseList,
        QCoreApplication::translate("(Common)", "Yes"), qVariantFromValue(1));
    new MythUIButtonListItem(m_browseList,
        QCoreApplication::translate("(Common)", "No"),  qVariantFromValue(0));
    m_browseList->SetValueByData(m_settings.GetBrowse());

    // Watched
    new MythUIButtonListItem(m_watchedList, tr("All", "Watched"),
                             kWatchedFilterAll);
    new MythUIButtonListItem(m_watchedList,
        QCoreApplication::translate("(Common)", "Yes"), qVariantFromValue(1));
    new MythUIButtonListItem(m_watchedList,
        QCoreApplication::translate("(Common)", "No"), qVariantFromValue(0));
    m_watchedList->SetValueByData(m_settings.GetWatched());

    // Inet Reference
    new MythUIButtonListItem(m_inetrefList, tr("All", "Inet reference"),
                           kInetRefFilterAll);
    new MythUIButtonListItem(m_inetrefList, tr("Unknown", "Inet reference"),
                           kInetRefFilterUnknown);
    m_inetrefList->SetValueByData(m_settings.getInteRef());

    // Coverfile
    new MythUIButtonListItem(m_coverfileList, tr("All", "Cover file"),
                           kCoverFileFilterAll);
    new MythUIButtonListItem(m_coverfileList, tr("None", "Cover file"),
                           kCoverFileFilterNone);
    m_coverfileList->SetValueByData(m_settings.GetCoverFile());

    // Order by
    new MythUIButtonListItem(m_orderbyList,
        QCoreApplication::translate("(Common)", "Title"),
        VideoFilterSettings::kOrderByTitle);
    new MythUIButtonListItem(m_orderbyList,
        QCoreApplication::translate("(Common)", "Season/Episode"),
        VideoFilterSettings::kOrderBySeasonEp);
    new MythUIButtonListItem(m_orderbyList,
        QCoreApplication::translate("(Common)", "Year"),
        VideoFilterSettings::kOrderByYearDescending);
    new MythUIButtonListItem(m_orderbyList,
        QCoreApplication::translate("(Common)", "User Rating"),
        VideoFilterSettings::kOrderByUserRatingDescending);
    new MythUIButtonListItem(m_orderbyList,
        QCoreApplication::translate("(Common)", "Runtime"),
        VideoFilterSettings::kOrderByLength);
    new MythUIButtonListItem(m_orderbyList,
        QCoreApplication::translate("(Common)", "Filename"),
        VideoFilterSettings::kOrderByFilename);
    new MythUIButtonListItem(m_orderbyList, tr("Video ID"),
        VideoFilterSettings::kOrderByID);
    new MythUIButtonListItem(m_orderbyList, tr("Date Added"),
        VideoFilterSettings::kOrderByDateAddedDescending);
    m_orderbyList->SetValueByData(m_settings.getOrderby());

    // Text Filter
    m_textfilter->SetText(m_settings.getTextFilter());
}

void VideoFilterDialog::saveAsDefault()
{
     m_settings.saveAsDefault();
     saveAndExit();
}

void VideoFilterDialog::saveAndExit()
{
    m_fsp->setSettings(m_settings);

    if (m_settings.getChangedState() > 0)
        emit filterChanged();
    Close();
}

void VideoFilterDialog::SetYear(MythUIButtonListItem *item)
{
    int new_year = item->GetData().toInt();
    m_settings.SetYear(new_year);
    update_numvideo();
}

void VideoFilterDialog::SetUserRating(MythUIButtonListItem *item)
{
    m_settings.SetUserRating(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::SetCategory(MythUIButtonListItem *item)
{
    m_settings.SetCategory(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::setCountry(MythUIButtonListItem *item)
{
    m_settings.setCountry(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::setGenre(MythUIButtonListItem *item)
{
    m_settings.setGenre(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::SetCast(MythUIButtonListItem *item)
{
    m_settings.SetCast(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::setRunTime(MythUIButtonListItem *item)
{
    m_settings.setRuntime(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::SetBrowse(MythUIButtonListItem *item)
{
    m_settings.SetBrowse(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::SetWatched(MythUIButtonListItem *item)
{
    m_settings.SetWatched(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::SetInetRef(MythUIButtonListItem *item)
{
    m_settings.SetInetRef(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::SetCoverFile(MythUIButtonListItem *item)
{
    m_settings.SetCoverFile(item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::setOrderby(MythUIButtonListItem *item)
{
    m_settings
            .setOrderby((VideoFilterSettings::ordering)item->GetData().toInt());
    update_numvideo();
}

void VideoFilterDialog::setTextFilter()
{
    m_settings.setTextFilter(m_textfilter->GetText());
    update_numvideo();
}
