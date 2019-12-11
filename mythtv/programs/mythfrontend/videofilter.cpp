#include <set>

#include "mythcontext.h"

#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "mythmiscutil.h"
#include "globals.h"
#include "dbaccess.h"
#include "videometadatalistmanager.h"
#include "videoutils.h"
#include "mythdate.h"

#include "videolist.h"
#include "videofilter.h"

VideoFilterSettings::VideoFilterSettings(bool loaddefaultsettings,
                                         const QString& _prefix)
{
    if (_prefix.isEmpty())
        m_prefix = "VideoDefault";
    else
        m_prefix = _prefix + "Default";

    // do nothing yet
    if (loaddefaultsettings)
    {
        m_category = gCoreContext->GetNumSetting(QString("%1Category").arg(m_prefix),
                                           kCategoryFilterAll);
        m_genre = gCoreContext->GetNumSetting(QString("%1Genre").arg(m_prefix),
                                        kGenreFilterAll);
        m_country = gCoreContext->GetNumSetting(QString("%1Country").arg(m_prefix),
                                          kCountryFilterAll);
        m_cast = gCoreContext->GetNumSetting(QString("%1Cast").arg(m_prefix),
                                        kCastFilterAll);
        m_year = gCoreContext->GetNumSetting(QString("%1Year").arg(m_prefix),
                                       kYearFilterAll);
        m_runtime = gCoreContext->GetNumSetting(QString("%1Runtime").arg(m_prefix),
                                          kRuntimeFilterAll);
        m_userrating =
                gCoreContext->GetNumSetting(QString("%1Userrating").arg(m_prefix),
                                        kUserRatingFilterAll);
        m_browse = gCoreContext->GetNumSetting(QString("%1Browse").arg(m_prefix),
                                         kBrowseFilterAll);
        m_watched = gCoreContext->GetNumSetting(QString("%1Watched").arg(m_prefix),
                                         kWatchedFilterAll);
        m_inetref = gCoreContext->GetNumSetting(QString("%1InetRef").arg(m_prefix),
                kInetRefFilterAll);
        m_coverfile = gCoreContext->GetNumSetting(QString("%1CoverFile")
                .arg(m_prefix), kCoverFileFilterAll);
        m_orderby = (ordering)gCoreContext->GetNumSetting(QString("%1Orderby")
                                                    .arg(m_prefix),
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
    if (this == &rhs)
        return *this;

    m_prefix = rhs.m_prefix;

    if (m_category != rhs.m_category)
    {
        m_changed_state |= kFilterCategoryChanged;
        m_category = rhs.m_category;
    }

    if (m_genre != rhs.m_genre)
    {
        m_changed_state |= kFilterGenreChanged;
        m_genre = rhs.m_genre;
    }

    if (m_country != rhs.m_country)
    {
        m_changed_state |= kFilterCountryChanged;
        m_country = rhs.m_country;
    }

    if (m_cast != rhs.m_cast)
    {
        m_changed_state |= kFilterCastChanged;
        m_cast = rhs.m_cast;
    }

    if (m_year != rhs.m_year)
    {
        m_changed_state |= kFilterYearChanged;
        m_year = rhs.m_year;
    }

    if (m_runtime != rhs.m_runtime)
    {
        m_changed_state |= kFilterRuntimeChanged;
        m_runtime = rhs.m_runtime;
    }

    if (m_userrating != rhs.m_userrating)
    {
        m_changed_state |= kFilterUserRatingChanged;
        m_userrating = rhs.m_userrating;
    }

    if (m_browse != rhs.m_browse)
    {
        m_changed_state |= kFilterBrowseChanged;
        m_browse = rhs.m_browse;
    }

    if (m_watched != rhs.m_watched)
    {
        m_changed_state |= kFilterWatchedChanged;
        m_watched = rhs.m_watched;
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

    if (m_orderby != rhs.m_orderby)
    {
        m_changed_state |= kSortOrderChanged;
        m_orderby = rhs.m_orderby;
    }

    if (m_parental_level != rhs.m_parental_level)
    {
        m_changed_state |= kFilterParentalLevelChanged;
        m_parental_level = rhs.m_parental_level;
    }

    if (m_textfilter != rhs.m_textfilter)
    {
        m_textfilter = rhs.m_textfilter;
        m_changed_state |= kFilterTextFilterChanged;
    }
    if (m_season != rhs.m_season || m_episode != rhs.m_episode)
    {
        m_season = rhs.m_season;
        m_episode = rhs.m_episode;
        m_changed_state |= kFilterTextFilterChanged;
    }
    if (m_insertdate != rhs.m_insertdate)
    {
        m_insertdate = rhs.m_insertdate;
        m_changed_state |= kFilterTextFilterChanged;
    }

    return *this;
}

void VideoFilterSettings::saveAsDefault()
{
    gCoreContext->SaveSetting(QString("%1Category").arg(m_prefix), m_category);
    gCoreContext->SaveSetting(QString("%1Genre").arg(m_prefix), m_genre);
    gCoreContext->SaveSetting(QString("%1Cast").arg(m_prefix), m_cast);
    gCoreContext->SaveSetting(QString("%1Country").arg(m_prefix), m_country);
    gCoreContext->SaveSetting(QString("%1Year").arg(m_prefix), m_year);
    gCoreContext->SaveSetting(QString("%1Runtime").arg(m_prefix), m_runtime);
    gCoreContext->SaveSetting(QString("%1Userrating").arg(m_prefix), m_userrating);
    gCoreContext->SaveSetting(QString("%1Browse").arg(m_prefix), m_browse);
    gCoreContext->SaveSetting(QString("%1Watched").arg(m_prefix), m_watched);
    gCoreContext->SaveSetting(QString("%1InetRef").arg(m_prefix), m_inetref);
    gCoreContext->SaveSetting(QString("%1CoverFile").arg(m_prefix), m_coverfile);
    gCoreContext->SaveSetting(QString("%1Orderby").arg(m_prefix), m_orderby);
    gCoreContext->SaveSetting(QString("%1Filter").arg(m_prefix), m_textfilter);
}

bool VideoFilterSettings::matches_filter(const VideoMetadata &mdata) const
{
    bool matches = true;

    //textfilter
    if (!m_textfilter.isEmpty())
    {
        matches = false;
        matches = (matches ||
                   mdata.GetTitle().contains(m_textfilter, Qt::CaseInsensitive));
        matches = (matches ||
                   mdata.GetSubtitle().contains(m_textfilter, Qt::CaseInsensitive));
        matches = (matches ||
                   mdata.GetPlot().contains(m_textfilter, Qt::CaseInsensitive));
    }
    //search for season with optionally episode nr.
    if (matches && (m_season != -1))
    {
        matches = (m_season == mdata.GetSeason());
        matches = (matches && (m_episode == -1 || m_episode == mdata.GetEpisode()));
    }
    if (matches && m_insertdate.isValid())
    {
        matches = (mdata.GetInsertdate().isValid() &&
                   mdata.GetInsertdate() >= m_insertdate);
    }
    if (matches && (m_genre != kGenreFilterAll))
    {
        matches = false;

        const VideoMetadata::genre_list &gl = mdata.GetGenres();
        for (auto p = gl.cbegin(); p != gl.cend(); ++p)
        {
            if ((matches = (p->first == m_genre)))
            {
                break;
            }
        }
    }

    if (matches && m_country != kCountryFilterAll)
    {
        matches = false;

        const VideoMetadata::country_list &cl = mdata.GetCountries();
        for (auto p = cl.cbegin(); p != cl.cend(); ++p)
        {
            if ((matches = (p->first == m_country)))
            {
                break;
            }
        }
    }

    if (matches && m_cast != kCastFilterAll)
    {
        const VideoMetadata::cast_list &cl = mdata.GetCast();

        if ((m_cast == kCastFilterUnknown) && (cl.empty()))
        {
            matches = true;
        }
        else
        {
            matches = false;

            for (auto p = cl.cbegin(); p != cl.cend(); ++p)
            {
                if ((matches = (p->first == m_cast)))
                {
                    break;
                }
            }
        }
    }

    if (matches && m_category != kCategoryFilterAll)
    {
        matches = (m_category == mdata.GetCategoryID());
    }

    if (matches && m_year != kYearFilterAll)
    {
        if (m_year == kYearFilterUnknown)
        {
            matches = ((mdata.GetYear() == 0) ||
                       (mdata.GetYear() == VIDEO_YEAR_DEFAULT));
        }
        else
        {
            matches = (m_year == mdata.GetYear());
        }
    }

    if (matches && m_runtime != kRuntimeFilterAll)
    {
        if (m_runtime == kRuntimeFilterUnknown)
        {
            matches = (mdata.GetLength() == 0);
        }
        else
        {
            matches = (m_runtime == (mdata.GetLength() / 30));
        }
    }

    if (matches && m_userrating != kUserRatingFilterAll)
    {
        matches = (mdata.GetUserRating() >= m_userrating);
    }

    if (matches && m_browse != kBrowseFilterAll)
    {
        matches = (mdata.GetBrowse() == m_browse);
    }

    if (matches && m_watched != kWatchedFilterAll)
    {
        matches = (mdata.GetWatched() == m_watched);
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
                                         const VideoMetadata &rhs) const
{
    bool ret = false;
    switch (m_orderby)
    {
        case kOrderByTitle:
        {
            ret = lhs.sortBefore(rhs);
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
                ret = lhs.sortBefore(rhs);
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
            const QString& lhsfn = lhs.GetSortFilename();
            const QString& rhsfn = rhs.GetSortFilename();
            ret = naturalCompare(lhsfn, rhsfn) < 0;
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
                    .arg(m_orderby));
        }
    }

    return ret;
}

void VideoFilterSettings::setTextFilter(const QString& val)
{
    m_changed_state |= kFilterTextFilterChanged;
    if (m_re_season.indexIn(val) != -1)
    {
        bool res = false;
        QStringList list = m_re_season.capturedTexts();
        m_season = list[1].toInt(&res);
        if (!res)
            m_season = -1;
        if (list.size() > 2) {
            m_episode = list[2].toInt(&res);
            if (!res)
                m_episode = -1;
        }
        else {
            m_episode = -1;
        }
        //clear \dX\d from string for string-search in plot/title/subtitle
        m_textfilter = val;
        m_textfilter.replace(m_re_season, "");
        m_textfilter = m_textfilter.simplified ();
    }
    else
    {
        m_textfilter = val;
        m_season = -1;
        m_episode = -1;
    }
    if (m_re_date.indexIn(m_textfilter) != -1)
    {
        QStringList list = m_re_date.capturedTexts();
        int modnr = list[1].toInt();
        QDate testdate = MythDate::current().date();
        switch(list[2].at(0).toLatin1())
        {
            case 'm': testdate = testdate.addMonths(-modnr);break;
            case 'd': testdate = testdate.addDays(-modnr);break;
            case 'w': testdate = testdate.addDays(-modnr * 7);break;
        }
        m_insertdate = testdate;
        m_textfilter.replace(m_re_date, "");
        m_textfilter = m_textfilter.simplified ();
    }
    else
    {
        //reset testdate
        m_insertdate = QDate();
    }
}

/////////////////////////////////
// VideoFilterDialog
/////////////////////////////////
VideoFilterDialog::VideoFilterDialog(MythScreenStack *lparent, const QString& lname,
        VideoList *video_list) : MythScreenType(lparent, lname),
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

    using int_list = std::set<int>;
    int_list years;
    int_list runtimes;
    int_list user_ratings;

    const VideoMetadataListManager::metadata_list &mdl =
            m_videoList.getListCache().getList();
    for (auto p = mdl.cbegin(); p != mdl.cend(); ++p)
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
    for (auto p = vcl.cbegin(); p != vcl.cend(); ++p)
    {
        new MythUIButtonListItem(m_categoryList, p->second, p->first);
    }

    new MythUIButtonListItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN,
                           kCategoryFilterUnknown);
    m_categoryList->SetValueByData(m_settings.GetCategory());

    // Genre
    new MythUIButtonListItem(m_genreList, tr("All", "Genre"), kGenreFilterAll);

    const VideoGenre::entry_list &gl = VideoGenre::getGenre().getList();
    for (auto p = gl.cbegin(); p != gl.cend(); ++p)
    {
        new MythUIButtonListItem(m_genreList, p->second, p->first);
    }

    new MythUIButtonListItem(m_genreList, VIDEO_GENRE_UNKNOWN, kGenreFilterUnknown);
    m_genreList->SetValueByData(m_settings.getGenre());

    // Cast
    new MythUIButtonListItem(m_castList, tr("All", "Cast"), kCastFilterAll);

    const VideoCast::entry_list &cl = VideoCast::GetCast().getList();
    for (auto p = cl.cbegin(); p != cl.cend(); ++p)
    {
        new MythUIButtonListItem(m_castList, p->second, p->first);
    }

    new MythUIButtonListItem(m_castList, VIDEO_CAST_UNKNOWN, kCastFilterUnknown);
    m_castList->SetValueByData(m_settings.GetCast());

    // Country
    new MythUIButtonListItem(m_countryList, tr("All", "Country"), kCountryFilterAll);

    const VideoCountry::entry_list &cnl = VideoCountry::getCountry().getList();
    for (auto p = cnl.cbegin(); p != cnl.cend(); ++p)
    {
        new MythUIButtonListItem(m_countryList, p->second, p->first);
    }

    new MythUIButtonListItem(m_countryList, VIDEO_COUNTRY_UNKNOWN,
                           kCountryFilterUnknown);
    m_countryList->SetValueByData(m_settings.getCountry());

    // Year
    new MythUIButtonListItem(m_yearList, tr("All", "Year"), kYearFilterAll);

    for (auto p = years.crbegin(); p != years.crend(); ++p)
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

    for (auto p = runtimes.cbegin(); p != runtimes.cend(); ++p)
    {
        QString s = QString("%1 %2 ~ %3 %4").arg(*p * 30).arg(tr("minutes"))
                .arg((*p + 1) * 30).arg(tr("minutes"));
        new MythUIButtonListItem(m_runtimeList, s, *p);
    }

    m_runtimeList->SetValueByData(m_settings.getRuntime());

    // User Rating
    new MythUIButtonListItem(m_userratingList, tr("All", "User rating"),
                           kUserRatingFilterAll);

    for (auto p = user_ratings.crbegin(); p != user_ratings.crend(); ++p)
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
