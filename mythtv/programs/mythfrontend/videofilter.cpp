// C++
#include <set>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/stringutil.h"
#include "libmythmetadata/dbaccess.h"
#include "libmythmetadata/globals.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythmetadata/videoutils.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitextedit.h"

// MythFrontend
#include "videofilter.h"
#include "videolist.h"

const QRegularExpression VideoFilterSettings::kReSeason {
    "(\\d+)x(\\d*)", QRegularExpression::CaseInsensitiveOption };
const QRegularExpression VideoFilterSettings::kReDate {
    "-(\\d+)([dwmy])", QRegularExpression::CaseInsensitiveOption };

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
        m_userRating =
                gCoreContext->GetNumSetting(QString("%1Userrating").arg(m_prefix),
                                        kUserRatingFilterAll);
        m_browse = gCoreContext->GetNumSetting(QString("%1Browse").arg(m_prefix),
                                         kBrowseFilterAll);
        m_watched = gCoreContext->GetNumSetting(QString("%1Watched").arg(m_prefix),
                                         kWatchedFilterAll);
        m_inetRef = gCoreContext->GetNumSetting(QString("%1InetRef").arg(m_prefix),
                kInetRefFilterAll);
        m_coverFile = gCoreContext->GetNumSetting(QString("%1CoverFile")
                .arg(m_prefix), kCoverFileFilterAll);
        m_orderBy = (ordering)gCoreContext->GetNumSetting(QString("%1Orderby")
                                                    .arg(m_prefix),
                                                    kOrderByTitle);
    }
}

VideoFilterSettings::VideoFilterSettings(const VideoFilterSettings &rhs)
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
        m_changedState |= kFilterCategoryChanged;
        m_category = rhs.m_category;
    }

    if (m_genre != rhs.m_genre)
    {
        m_changedState |= kFilterGenreChanged;
        m_genre = rhs.m_genre;
    }

    if (m_country != rhs.m_country)
    {
        m_changedState |= kFilterCountryChanged;
        m_country = rhs.m_country;
    }

    if (m_cast != rhs.m_cast)
    {
        m_changedState |= kFilterCastChanged;
        m_cast = rhs.m_cast;
    }

    if (m_year != rhs.m_year)
    {
        m_changedState |= kFilterYearChanged;
        m_year = rhs.m_year;
    }

    if (m_runtime != rhs.m_runtime)
    {
        m_changedState |= kFilterRuntimeChanged;
        m_runtime = rhs.m_runtime;
    }

    if (m_userRating != rhs.m_userRating)
    {
        m_changedState |= kFilterUserRatingChanged;
        m_userRating = rhs.m_userRating;
    }

    if (m_browse != rhs.m_browse)
    {
        m_changedState |= kFilterBrowseChanged;
        m_browse = rhs.m_browse;
    }

    if (m_watched != rhs.m_watched)
    {
        m_changedState |= kFilterWatchedChanged;
        m_watched = rhs.m_watched;
    }

    if (m_inetRef != rhs.m_inetRef)
    {
        m_changedState |= kFilterInetRefChanged;
        m_inetRef = rhs.m_inetRef;
    }

    if (m_coverFile != rhs.m_coverFile)
    {
        m_changedState |= kFilterCoverFileChanged;
        m_coverFile = rhs.m_coverFile;
    }

    if (m_orderBy != rhs.m_orderBy)
    {
        m_changedState |= kSortOrderChanged;
        m_orderBy = rhs.m_orderBy;
    }

    if (m_parentalLevel != rhs.m_parentalLevel)
    {
        m_changedState |= kFilterParentalLevelChanged;
        m_parentalLevel = rhs.m_parentalLevel;
    }

    if (m_textFilter != rhs.m_textFilter)
    {
        m_textFilter = rhs.m_textFilter;
        m_changedState |= kFilterTextFilterChanged;
    }
    if (m_season != rhs.m_season || m_episode != rhs.m_episode)
    {
        m_season = rhs.m_season;
        m_episode = rhs.m_episode;
        m_changedState |= kFilterTextFilterChanged;
    }
    if (m_insertDate != rhs.m_insertDate)
    {
        m_insertDate = rhs.m_insertDate;
        m_changedState |= kFilterTextFilterChanged;
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
    gCoreContext->SaveSetting(QString("%1Userrating").arg(m_prefix), m_userRating);
    gCoreContext->SaveSetting(QString("%1Browse").arg(m_prefix), m_browse);
    gCoreContext->SaveSetting(QString("%1Watched").arg(m_prefix), m_watched);
    gCoreContext->SaveSetting(QString("%1InetRef").arg(m_prefix), m_inetRef);
    gCoreContext->SaveSetting(QString("%1CoverFile").arg(m_prefix), m_coverFile);
    gCoreContext->SaveSetting(QString("%1Orderby").arg(m_prefix), m_orderBy);
    gCoreContext->SaveSetting(QString("%1Filter").arg(m_prefix), m_textFilter);
}

bool VideoFilterSettings::matches_filter(const VideoMetadata &mdata) const
{
    bool matches = true;

    //textfilter
    if (!m_textFilter.isEmpty())
    {
        matches = false;
        matches = (matches ||
                   mdata.GetTitle().contains(m_textFilter, Qt::CaseInsensitive));
        matches = (matches ||
                   mdata.GetSubtitle().contains(m_textFilter, Qt::CaseInsensitive));
        matches = (matches ||
                   mdata.GetPlot().contains(m_textFilter, Qt::CaseInsensitive));
    }
    //search for season with optionally episode nr.
    if (matches && (m_season != -1))
    {
        matches = (m_season == mdata.GetSeason());
        matches = (matches && (m_episode == -1 || m_episode == mdata.GetEpisode()));
    }
    if (matches && m_insertDate.isValid())
    {
        matches = (mdata.GetInsertdate().isValid() &&
                   mdata.GetInsertdate() >= m_insertDate);
    }
    if (matches && (m_genre != kGenreFilterAll))
    {
        const VideoMetadata::genre_list &gl = mdata.GetGenres();
        auto samegenre = [this](const auto & g) {return g.first == m_genre; };
        matches = std::any_of(gl.cbegin(), gl.cend(), samegenre);
    }

    if (matches && m_country != kCountryFilterAll)
    {
        const VideoMetadata::country_list &cl = mdata.GetCountries();
        auto samecountry = [this](const auto & c) {return c.first == m_country; };
        matches = std::any_of(cl.cbegin(), cl.cend(), samecountry);
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
            auto samecast = [this](const auto & c){return c.first == m_cast; };
            matches = std::any_of(cl.cbegin(), cl.cend(), samecast);
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
            matches = (mdata.GetLength() == 0min);
        }
        else
        {
            matches = (m_runtime == (mdata.GetLength() / 30min));
        }
    }

    if (matches && m_userRating != kUserRatingFilterAll)
    {
        matches = (mdata.GetUserRating() >= m_userRating);
    }

    if (matches && m_browse != kBrowseFilterAll)
    {
        matches = (static_cast<int>(mdata.GetBrowse()) == m_browse);
    }

    if (matches && m_watched != kWatchedFilterAll)
    {
        matches = (static_cast<int>(mdata.GetWatched()) == m_watched);
    }

    if (matches && m_inetRef != kInetRefFilterAll)
    {
        matches = (mdata.GetInetRef() == VIDEO_INETREF_DEFAULT);
    }

    if (matches && m_coverFile != kCoverFileFilterAll)
    {
        matches = (IsDefaultCoverFile(mdata.GetCoverFile()));
    }

    if (matches && m_parentalLevel)
    {
        matches = ((mdata.GetShowLevel() != ParentalLevel::plNone) &&
                (mdata.GetShowLevel() <= m_parentalLevel));
    }

    return matches;
}

/// Compares two VideoMetadata instances
bool VideoFilterSettings::meta_less_than(const VideoMetadata &lhs,
                                         const VideoMetadata &rhs) const
{
    switch (m_orderBy)
    {
        case kOrderByTitle:                 return lhs.sortBefore(rhs);
        case kOrderByYearDescending:        return lhs.GetYear()        > rhs.GetYear();
        case kOrderByUserRatingDescending:  return lhs.GetUserRating()  > rhs.GetUserRating();
        case kOrderByDateAddedDescending:   return lhs.GetInsertdate()  > rhs.GetInsertdate();
        case kOrderByLength:                return lhs.GetLength()      < rhs.GetLength();
        case kOrderByID:                    return lhs.GetID()          < rhs.GetID();
        case kOrderByFilename:
            return StringUtil::naturalSortCompare(lhs.GetSortFilename(), rhs.GetSortFilename());
        case kOrderBySeasonEp:
        {
            if ((lhs.GetSeason() == rhs.GetSeason())
                && (lhs.GetEpisode() == rhs.GetEpisode())
                && (lhs.GetSeason() == 0)
                && (rhs.GetSeason() == 0)
                && (lhs.GetEpisode() == 0)
                && (rhs.GetEpisode() == 0))
            {
                return lhs.sortBefore(rhs);
            }
            if ((lhs.GetSeason() == rhs.GetSeason())
                && (lhs.GetTitle() == rhs.GetTitle()))
            {
                return (lhs.GetEpisode() < rhs.GetEpisode());
            }
            return (lhs.GetSeason() < rhs.GetSeason());
        }
        default:
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error: unknown sort type %1")
                    .arg(m_orderBy));
            return false;
        }
    }
}

void VideoFilterSettings::setTextFilter(const QString& val)
{
    m_changedState |= kFilterTextFilterChanged;
    auto match = kReSeason.match(val);
    if (match.hasMatch())
    {
        m_season = match.capturedView(1).isEmpty()
            ? -1 : match.capturedView(1).toInt();
        m_episode = match.capturedView(2).isEmpty()
            ? -1 : match.capturedView(2).toInt();
        //clear \dX\d from string for string-search in plot/title/subtitle
        m_textFilter = val;
        m_textFilter.remove(match.capturedStart(), match.capturedLength());
        m_textFilter = m_textFilter.simplified ();
    }
    else
    {
        m_textFilter = val;
        m_season = -1;
        m_episode = -1;
    }
    match = kReDate.match(m_textFilter);
    if (match.hasMatch())
    {
        int64_t modnr = match.capturedView(1).toInt();
        QDate testdate = MythDate::current().date();
        switch(match.capturedView(2).at(0).toLatin1())
        {
            case 'd': testdate = testdate.addDays(-modnr);break;
            case 'w': testdate = testdate.addDays(-modnr * 7);break;
            case 'm': testdate = testdate.addMonths(-modnr);break;
            case 'y': testdate = testdate.addYears(-modnr);break;
        }
        m_insertDate = testdate;
        m_textFilter.remove(match.capturedStart(), match.capturedLength());
        m_textFilter = m_textFilter.simplified ();
    }
    else
    {
        //reset testdate
        m_insertDate = QDate();
    }
}

/////////////////////////////////
// VideoFilterDialog
/////////////////////////////////
VideoFilterDialog::VideoFilterDialog(MythScreenStack *lparent, const QString& lname,
        VideoList *video_list) : MythScreenType(lparent, lname),
    m_videoList(*video_list),
    m_fsp(new BasicFilterSettingsProxy<VideoList>(*video_list))
{
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
    UIUtilE::Assign(this, m_textFilter, "textfilter_input", &err);
    UIUtilE::Assign(this, m_yearList, "year_select", &err);
    UIUtilE::Assign(this, m_userRatingList, "userrating_select", &err);
    UIUtilE::Assign(this, m_categoryList, "category_select", &err);
    UIUtilE::Assign(this, m_countryList, "country_select", &err);
    UIUtilE::Assign(this, m_genreList, "genre_select", &err);
    UIUtilE::Assign(this, m_castList, "cast_select", &err);
    UIUtilE::Assign(this, m_runtimeList, "runtime_select", &err);
    UIUtilE::Assign(this, m_browseList, "browse_select", &err);
    UIUtilE::Assign(this, m_watchedList, "watched_select", &err);
    UIUtilE::Assign(this, m_inetRefList, "inetref_select", &err);
    UIUtilE::Assign(this, m_coverFileList, "coverfile_select", &err);
    UIUtilE::Assign(this, m_orderByList, "orderby_select", &err);

    UIUtilE::Assign(this, m_doneButton, "done_button", &err);
    UIUtilE::Assign(this, m_saveButton, "save_button", &err);

    UIUtilE::Assign(this, m_numVideosText, "numvideos_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'filter'");
        return false;
    }

    BuildFocusList();

    fillWidgets();
    update_numvideo();

    connect(m_yearList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetYear);
    connect(m_userRatingList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetUserRating);
    connect(m_categoryList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetCategory);
    connect(m_countryList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::setCountry);
    connect(m_genreList,&MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::setGenre);
    connect(m_castList,&MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetCast);
    connect(m_runtimeList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::setRunTime);
    connect(m_browseList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetBrowse);
    connect(m_watchedList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetWatched);
    connect(m_inetRefList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetInetRef);
    connect(m_coverFileList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::SetCoverFile);
    connect(m_orderByList, &MythUIButtonList::itemSelected,
            this, &VideoFilterDialog::setOrderby);
    connect(m_textFilter, &MythUITextEdit::valueChanged,
            this, &VideoFilterDialog::setTextFilter);

    connect(m_saveButton, &MythUIButton::Clicked, this, &VideoFilterDialog::saveAsDefault);
    connect(m_doneButton, &MythUIButton::Clicked, this, &VideoFilterDialog::saveAndExit);

    return true;
}

void VideoFilterDialog::update_numvideo()
{
    int video_count = m_videoList.TryFilter(m_settings);

    if (video_count > 0)
    {
        m_numVideosText->SetText(tr("Result of this filter : %n video(s)", "",
                                    video_count));
    }
    else
    {
        m_numVideosText->SetText(tr("Result of this filter : No Videos"));
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
    for (const auto & md : mdl)
    {
        int year = md->GetYear();
        if ((year == 0) || (year == VIDEO_YEAR_DEFAULT))
            have_unknown_year = true;
        else
            years.insert(year);

        std::chrono::minutes runtime = md->GetLength();
        if (runtime == 0min)
            have_unknown_runtime = true;
        else
            runtimes.insert(runtime.count() / 30);

        user_ratings.insert(static_cast<int>(md->GetUserRating()));
    }

    // Category
    new MythUIButtonListItem(m_categoryList, tr("All", "Category"),
                           kCategoryFilterAll);

    const VideoCategory::entry_list &vcl =
            VideoCategory::GetCategory().getList();
    for (const auto & vc : vcl)
        new MythUIButtonListItem(m_categoryList, vc.second, vc.first);

    new MythUIButtonListItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN,
                           kCategoryFilterUnknown);
    m_categoryList->SetValueByData(m_settings.GetCategory());

    // Genre
    new MythUIButtonListItem(m_genreList, tr("All", "Genre"), kGenreFilterAll);

    const VideoGenre::entry_list &gl = VideoGenre::getGenre().getList();
    for (const auto & g : gl)
        new MythUIButtonListItem(m_genreList, g.second, g.first);

    new MythUIButtonListItem(m_genreList, VIDEO_GENRE_UNKNOWN, kGenreFilterUnknown);
    m_genreList->SetValueByData(m_settings.getGenre());

    // Cast
    new MythUIButtonListItem(m_castList, tr("All", "Cast"), kCastFilterAll);

    const VideoCast::entry_list &cl = VideoCast::GetCast().getList();
    for (const auto & c : cl)
        new MythUIButtonListItem(m_castList, c.second, c.first);

    new MythUIButtonListItem(m_castList, VIDEO_CAST_UNKNOWN, kCastFilterUnknown);
    m_castList->SetValueByData(m_settings.GetCast());

    // Country
    new MythUIButtonListItem(m_countryList, tr("All", "Country"), kCountryFilterAll);

    const VideoCountry::entry_list &cnl = VideoCountry::getCountry().getList();
    for (const auto & cn : cnl)
        new MythUIButtonListItem(m_countryList, cn.second, cn.first);

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

    for (int runtime : runtimes)
    {
        QString s = QString("%1 %2 ~ %3 %4").arg(runtime * 30).arg(tr("minutes"))
                .arg((runtime + 1) * 30).arg(tr("minutes"));
        new MythUIButtonListItem(m_runtimeList, s, runtime);
    }

    m_runtimeList->SetValueByData(m_settings.getRuntime());

    // User Rating
    new MythUIButtonListItem(m_userRatingList, tr("All", "User rating"),
                           kUserRatingFilterAll);

    for (auto p = user_ratings.crbegin(); p != user_ratings.crend(); ++p)
    {
        new MythUIButtonListItem(m_userRatingList,
                               QString(">= %1").arg(QString::number(*p)),
                               *p);
    }

    m_userRatingList->SetValueByData(m_settings.GetUserRating());

    // Browsable
    new MythUIButtonListItem(m_browseList, tr("All", "Browsable"),
                             kBrowseFilterAll);
    new MythUIButtonListItem(m_browseList,
        QCoreApplication::translate("(Common)", "Yes"), QVariant::fromValue(1));
    new MythUIButtonListItem(m_browseList,
        QCoreApplication::translate("(Common)", "No"),  QVariant::fromValue(0));
    m_browseList->SetValueByData(m_settings.GetBrowse());

    // Watched
    new MythUIButtonListItem(m_watchedList, tr("All", "Watched"),
                             kWatchedFilterAll);
    new MythUIButtonListItem(m_watchedList,
        QCoreApplication::translate("(Common)", "Yes"), QVariant::fromValue(1));
    new MythUIButtonListItem(m_watchedList,
        QCoreApplication::translate("(Common)", "No"), QVariant::fromValue(0));
    m_watchedList->SetValueByData(m_settings.GetWatched());

    // Inet Reference
    new MythUIButtonListItem(m_inetRefList, tr("All", "Inet reference"),
                           kInetRefFilterAll);
    new MythUIButtonListItem(m_inetRefList, tr("Unknown", "Inet reference"),
                           kInetRefFilterUnknown);
    m_inetRefList->SetValueByData(m_settings.getInteRef());

    // Coverfile
    new MythUIButtonListItem(m_coverFileList, tr("All", "Cover file"),
                           kCoverFileFilterAll);
    new MythUIButtonListItem(m_coverFileList, tr("None", "Cover file"),
                           kCoverFileFilterNone);
    m_coverFileList->SetValueByData(m_settings.GetCoverFile());

    // Order by
    new MythUIButtonListItem(m_orderByList,
        QCoreApplication::translate("(Common)", "Title"),
        VideoFilterSettings::kOrderByTitle);
    new MythUIButtonListItem(m_orderByList,
        QCoreApplication::translate("(Common)", "Season/Episode"),
        VideoFilterSettings::kOrderBySeasonEp);
    new MythUIButtonListItem(m_orderByList,
        QCoreApplication::translate("(Common)", "Year"),
        VideoFilterSettings::kOrderByYearDescending);
    new MythUIButtonListItem(m_orderByList,
        QCoreApplication::translate("(Common)", "User Rating"),
        VideoFilterSettings::kOrderByUserRatingDescending);
    new MythUIButtonListItem(m_orderByList,
        QCoreApplication::translate("(Common)", "Runtime"),
        VideoFilterSettings::kOrderByLength);
    new MythUIButtonListItem(m_orderByList,
        QCoreApplication::translate("(Common)", "Filename"),
        VideoFilterSettings::kOrderByFilename);
    new MythUIButtonListItem(m_orderByList, tr("Video ID"),
        VideoFilterSettings::kOrderByID);
    new MythUIButtonListItem(m_orderByList, tr("Date Added"),
        VideoFilterSettings::kOrderByDateAddedDescending);
    m_orderByList->SetValueByData(m_settings.getOrderby());

    // Text Filter
    m_textFilter->SetText(m_settings.getTextFilter());
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
    m_settings.setTextFilter(m_textFilter->GetText());
    update_numvideo();
}
