// C++ headers
#include <set>

// Myth headers
#include <mythtv/mythcontext.h>

// MythVideo headers
#include "globals.h"
#include "videofilter.h"
#include "videolist.h"
#include "dbaccess.h"
#include "metadata.h"
#include "metadatalistmanager.h"
#include "videoutils.h"

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

enum InetRefFilter {
    kInetRefFilterAll = -1,
    kInetRefFilterUnknown = 0
};

enum CoverFileFilter {
    kCoverFileFilterAll = -1,
    kCoverFileFilterNone = 0
};

const unsigned int VideoFilterSettings::FILTER_MASK;
const unsigned int VideoFilterSettings::SORT_MASK;

VideoFilterSettings::VideoFilterSettings(bool loaddefaultsettings,
                                         const QString& _prefix) :
    category(kCategoryFilterAll), genre(kGenreFilterAll),
    country(kCountryFilterAll), cast(kCastFilterAll),
    year(kYearFilterAll), runtime(kRuntimeFilterAll),
    userrating(kUserRatingFilterAll), browse(kBrowseFilterAll),
    m_inetref(kInetRefFilterAll), m_coverfile(kCoverFileFilterAll),
    orderby(kOrderByTitle), m_parental_level(ParentalLevel::plNone),
    m_changed_state(0)
{
    if (_prefix.isEmpty())
        prefix = "VideoDefault";
    else
        prefix = _prefix + "Default";

    // do nothing yet
    if (loaddefaultsettings)
    {
        category = gContext->GetNumSetting(QString("%1Category").arg(prefix),
                                           kCategoryFilterAll);
        genre = gContext->GetNumSetting(QString("%1Genre").arg(prefix),
                                        kGenreFilterAll);
        country = gContext->GetNumSetting(QString("%1Country").arg(prefix),
                                          kCountryFilterAll);
        cast = gContext->GetNumSetting(QString("%1Cast").arg(prefix),
                                        kCastFilterAll);
        year = gContext->GetNumSetting(QString("%1Year").arg(prefix),
                                       kYearFilterAll);
        runtime = gContext->GetNumSetting(QString("%1Runtime").arg(prefix),
                                          kRuntimeFilterAll);
        userrating =
                gContext->GetNumSetting(QString("%1Userrating").arg(prefix),
                                        kUserRatingFilterAll);
        browse = gContext->GetNumSetting(QString("%1Browse").arg(prefix),
                                         kBrowseFilterAll);
        m_inetref = gContext->GetNumSetting(QString("%1InetRef").arg(prefix),
                kInetRefFilterAll);
        m_coverfile = gContext->GetNumSetting(QString("%1CoverFile")
                .arg(prefix), kCoverFileFilterAll);
        orderby = (ordering)gContext->GetNumSetting(QString("%1Orderby")
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

    return *this;
}

void VideoFilterSettings::saveAsDefault()
{
    gContext->SaveSetting(QString("%1Category").arg(prefix), category);
    gContext->SaveSetting(QString("%1Genre").arg(prefix), genre);
    gContext->SaveSetting(QString("%1Cast").arg(prefix), cast);
    gContext->SaveSetting(QString("%1Country").arg(prefix), country);
    gContext->SaveSetting(QString("%1Year").arg(prefix), year);
    gContext->SaveSetting(QString("%1Runtime").arg(prefix), runtime);
    gContext->SaveSetting(QString("%1Userrating").arg(prefix), userrating);
    gContext->SaveSetting(QString("%1Browse").arg(prefix), browse);
    gContext->SaveSetting(QString("%1InetRef").arg(prefix), m_inetref);
    gContext->SaveSetting(QString("%1CoverFile").arg(prefix), m_coverfile);
    gContext->SaveSetting(QString("%1Orderby").arg(prefix), orderby);
}

bool VideoFilterSettings::matches_filter(const Metadata &mdata) const
{
    bool matches = true;

    if (genre != kGenreFilterAll)
    {
        matches = false;

        const Metadata::genre_list &gl = mdata.Genres();
        for (Metadata::genre_list::const_iterator p = gl.begin();
             p != gl.end(); ++p)
        {
            if ((matches = p->first == genre))
            {
                break;
            }
        }
    }

    if (matches && country != kCountryFilterAll)
    {
        matches = false;

        const Metadata::country_list &cl = mdata.Countries();
        for (Metadata::country_list::const_iterator p = cl.begin();
             p != cl.end(); ++p)
        {
            if ((matches = p->first == country))
            {
                break;
            }
        }
    }

    if (matches && cast != kCastFilterAll)
    {
        const Metadata::cast_list &cl = mdata.getCast();

        if (cast == kCastFilterUnknown && cl.size() == 0)
        {
            matches = true;
        }
        else
        {
            matches = false;

            for (Metadata::cast_list::const_iterator p = cl.begin();
                 p != cl.end(); ++p)
            {
                if ((matches = p->first == cast))
                {
                    break;
                }
            }
        }
    }

    if (matches && category != kCategoryFilterAll)
    {
        matches = category == mdata.getCategoryID();
    }

    if (matches && year != kYearFilterAll)
    {
        if (year == kYearFilterUnknown)
        {
            matches = (mdata.Year() == 0) ||
                    (mdata.Year() == VIDEO_YEAR_DEFAULT);
        }
        else
        {
            matches = year == mdata.Year();
        }
    }

    if (matches && runtime != kRuntimeFilterAll)
    {
        if (runtime == kRuntimeFilterUnknown)
        {
            matches = mdata.Length() < 0;
        }
        else
        {
            matches = runtime == (mdata.Length() / 30);
        }
    }

    if (matches && userrating != kUserRatingFilterAll)
    {
        matches = mdata.UserRating() >= userrating;
    }

    if (matches && browse != kBrowseFilterAll)
    {
        matches = mdata.Browse() == browse;
    }

    if (matches && m_inetref != kInetRefFilterAll)
    {
        matches = mdata.InetRef() == VIDEO_INETREF_DEFAULT;
    }

    if (matches && m_coverfile != kCoverFileFilterAll)
    {
        matches = isDefaultCoverFile(mdata.CoverFile());
    }

    if (matches && m_parental_level)
    {
        matches = (mdata.ShowLevel() != ParentalLevel::plNone) &&
                (mdata.ShowLevel() <= m_parental_level);
    }

    return matches;
}

/// Compares two Metadata instances
bool VideoFilterSettings::meta_less_than(const Metadata &lhs,
                                         const Metadata &rhs,
                                         bool sort_ignores_case) const
{
    bool ret = false;
    switch (orderby)
    {
        case kOrderByTitle:
        {
            Metadata::SortKey lhs_key;
            Metadata::SortKey rhs_key;
            if (lhs.hasSortKey() && rhs.hasSortKey())
            {
                lhs_key = lhs.getSortKey();
                rhs_key = rhs.getSortKey();
            }
            else
            {
                lhs_key = Metadata::GenerateDefaultSortKey(lhs,
                                                           sort_ignores_case);
                rhs_key = Metadata::GenerateDefaultSortKey(rhs,
                                                           sort_ignores_case);
            }
            ret = lhs_key < rhs_key;
            break;
        }
        case kOrderByYearDescending:
        {
            ret = lhs.Year() > rhs.Year();
            break;
        }
        case kOrderByUserRatingDescending:
        {
            ret = lhs.UserRating() > rhs.UserRating();
            break;
        }
        case kOrderByLength:
        {
            ret = lhs.Length() < rhs.Length();
            break;
        }
        case kOrderByFilename:
        {
            QString lhsfn(sort_ignores_case ?
                          lhs.Filename().toLower() : lhs.Filename());
            QString rhsfn(sort_ignores_case ?
                          rhs.Filename().toLower() : rhs.Filename());
            ret = QString::localeAwareCompare(lhsfn, rhsfn) < 0;
            break;
        }
        case kOrderByID:
        {
            ret = lhs.ID() < rhs.ID();
            break;
        }
        default:
        {
            VERBOSE(VB_IMPORTANT, QString("Error: unknown sort type %1")
                    .arg(orderby));
        }
    }

    return ret;
}

/////////////////////////////////
// VideoFilterDialog
/////////////////////////////////
VideoFilterDialog::VideoFilterDialog(MythScreenStack *parent, QString name,
                                     VideoList *video_list)
                  : MythScreenType(parent, name), m_videoList(*video_list)
{
    m_fsp = new BasicFilterSettingsProxy<VideoList>(*video_list);
    m_browseList = m_orderbyList = m_yearList = m_userratingList = NULL;
    m_categoryList =  m_countryList = m_genreList = m_castList = NULL;
    m_runtimeList = m_inetrefList = m_coverfileList = NULL;
    m_saveButton = m_doneButton = NULL;
    m_numvideosText = NULL;

    m_settings = m_fsp->getSettings();
}

VideoFilterDialog::~VideoFilterDialog()
{
    if (m_fsp)
        delete m_fsp;
}

bool VideoFilterDialog::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "filter", this);

    if (!foundtheme)
        return false;

    m_yearList = dynamic_cast<MythListButton *> (GetChild("year_select"));
    m_userratingList = dynamic_cast<MythListButton *>
                                                (GetChild("userrating_select"));
    m_categoryList = dynamic_cast<MythListButton *>
                                                (GetChild("category_select"));
    m_countryList = dynamic_cast<MythListButton *> (GetChild("country_select"));
    m_genreList = dynamic_cast<MythListButton *> (GetChild("genre_select"));
    m_castList = dynamic_cast<MythListButton *> (GetChild("cast_select"));
    m_runtimeList = dynamic_cast<MythListButton *> (GetChild("runtime_select"));
    m_browseList = dynamic_cast<MythListButton *> (GetChild("browse_select"));
    m_inetrefList = dynamic_cast<MythListButton *> (GetChild("inetref_select"));
    m_coverfileList = dynamic_cast<MythListButton *>
                                                (GetChild("coverfile_select"));
    m_orderbyList = dynamic_cast<MythListButton *> (GetChild("orderby_select"));

    m_doneButton = dynamic_cast<MythUIButton *> (GetChild("done_button"));
    m_saveButton = dynamic_cast<MythUIButton *> (GetChild("save_button"));

    m_numvideosText = dynamic_cast<MythUIText *> (GetChild("numvideos_text"));

    if (!m_browseList || !m_orderbyList || !m_yearList || !m_userratingList ||
        !m_categoryList || ! m_countryList || !m_genreList || !m_castList ||
        !m_runtimeList || !m_inetrefList || !m_coverfileList ||
        !m_saveButton || !m_doneButton || !m_numvideosText)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    m_saveButton->SetText(tr("Save as default"));
    m_doneButton->SetText(tr("Done"));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    fillWidgets();
    update_numvideo();

    connect(m_yearList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setYear(MythListButtonItem*)));
    connect(m_userratingList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setUserRating(MythListButtonItem*)));
    connect(m_categoryList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setCategory(MythListButtonItem*)));
    connect(m_countryList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setCountry(MythListButtonItem*)));
    connect(m_genreList,SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setGenre(MythListButtonItem*)));
    connect(m_castList,SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setCast(MythListButtonItem*)));
    connect(m_runtimeList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setRunTime(MythListButtonItem*)));
    connect(m_browseList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setBrowse(MythListButtonItem*)));
    connect(m_inetrefList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setInetRef(MythListButtonItem*)));
    connect(m_coverfileList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setCoverFile(MythListButtonItem*)));
    connect(m_orderbyList, SIGNAL(itemSelected(MythListButtonItem*)),
            SLOT(setOrderby(MythListButtonItem*)));

    connect(m_saveButton, SIGNAL(buttonPressed()), SLOT(saveAsDefault()));
    connect(m_doneButton, SIGNAL(buttonPressed()), SLOT(saveAndExit()));

    return true;
}

void VideoFilterDialog::update_numvideo()
{
    int video_count = m_videoList.test_filter(m_settings);

    if (video_count > 0)
    {
        m_numvideosText->SetText(
                QString(tr("Result of this filter : %1 video(s)"))
                .arg(video_count));
    }
    else
    {
        m_numvideosText->SetText(
                QString(tr("Result of this filter : No Videos")));
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

    const MetadataListManager::metadata_list &mdl =
            m_videoList.getListCache().getList();
    for (MetadataListManager::metadata_list::const_iterator p = mdl.begin();
         p != mdl.end(); ++p)
    {
        int year = (*p)->Year();
        if ((year == 0) || (year == VIDEO_YEAR_DEFAULT))
            have_unknown_year = true;
        else
            years.insert(year);

        int runtime = (*p)->Length();
        if (runtime == 0)
            have_unknown_runtime = true;
        else
            runtimes.insert(runtime / 30);

        user_ratings.insert(static_cast<int>((*p)->UserRating()));
    }

    // Category
    new MythListButtonItem(m_categoryList, QObject::tr("All"),
                           kCategoryFilterAll);

    const VideoCategory::entry_list &vcl =
            VideoCategory::getCategory().getList();
    for (VideoCategory::entry_list::const_iterator p = vcl.begin();
            p != vcl.end(); ++p)
    {
        new MythListButtonItem(m_categoryList, p->second, p->first);
    }

    new MythListButtonItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN,
                           kCategoryFilterUnknown);
    m_categoryList->SetValueByData(m_settings.getCategory());

    // Genre
    new MythListButtonItem(m_genreList, QObject::tr("All"), kGenreFilterAll);

    const VideoGenre::entry_list &gl = VideoGenre::getGenre().getList();
    for (VideoGenre::entry_list::const_iterator p = gl.begin();
            p != gl.end(); ++p)
    {
        new MythListButtonItem(m_genreList, p->second, p->first);
    }

    new MythListButtonItem(m_genreList, VIDEO_GENRE_UNKNOWN, kGenreFilterUnknown);
    m_genreList->SetValueByData(m_settings.getGenre());

    // Cast
    new MythListButtonItem(m_castList, QObject::tr("All"), kCastFilterAll);

    const VideoCast::entry_list &cl = VideoCast::getCast().getList();
    for (VideoCast::entry_list::const_iterator p = cl.begin();
            p != cl.end(); ++p)
    {
        new MythListButtonItem(m_castList, p->second, p->first);
    }

    new MythListButtonItem(m_castList, VIDEO_CAST_UNKNOWN, kCastFilterUnknown);
    m_castList->SetValueByData(m_settings.getCast());

    // Country
    new MythListButtonItem(m_countryList, QObject::tr("All"), kCountryFilterAll);

    const VideoCountry::entry_list &cnl = VideoCountry::getCountry().getList();
    for (VideoCountry::entry_list::const_iterator p = cnl.begin();
            p != cnl.end(); ++p)
    {
        new MythListButtonItem(m_countryList, p->second, p->first);
    }

    new MythListButtonItem(m_countryList, VIDEO_COUNTRY_UNKNOWN,
                           kCountryFilterUnknown);
    m_countryList->SetValueByData(m_settings.getCountry());

    // Year
    new MythListButtonItem(m_yearList, QObject::tr("All"), kYearFilterAll);

    for (int_list::const_reverse_iterator p = years.rbegin();
            p != years.rend(); ++p)
    {
        new MythListButtonItem(m_yearList, QString::number(*p), *p);
    }

    if (have_unknown_year)
        new MythListButtonItem(m_yearList, VIDEO_YEAR_UNKNOWN,
                               kYearFilterUnknown);

    m_yearList->SetValueByData(m_settings.getYear());

    // Runtime
    new MythListButtonItem(m_runtimeList, QObject::tr("All"), kRuntimeFilterAll);

    if (have_unknown_runtime)
        new MythListButtonItem(m_runtimeList, VIDEO_RUNTIME_UNKNOWN,
                               kRuntimeFilterUnknown);

    for (int_list::const_iterator p = runtimes.begin();
            p != runtimes.end(); ++p)
    {
        QString s = QString("%1 %2 ~ %3 %4").arg(*p * 30).arg(tr("minutes"))
                .arg((*p + 1) * 30).arg(tr("minutes"));
        new MythListButtonItem(m_runtimeList, s, *p);
    }

    m_runtimeList->SetValueByData(m_settings.getRuntime());

    // User Rating
    new MythListButtonItem(m_userratingList, QObject::tr("All"),
                           kUserRatingFilterAll);

    for (int_list::const_reverse_iterator p = user_ratings.rbegin();
            p != user_ratings.rend(); ++p)
    {
        new MythListButtonItem(m_userratingList,
                               QString(">= %1").arg(QString::number(*p)),
                               *p);
    }

    m_userratingList->SetValueByData(m_settings.getUserrating());

    // Browsable
    new MythListButtonItem(m_browseList, QObject::tr("All"), kBrowseFilterAll);
    new MythListButtonItem(m_browseList, QObject::tr("Yes"), 1);
    new MythListButtonItem(m_browseList, QObject::tr("No"), 0);
    m_browseList->SetValueByData(m_settings.getBrowse());

    // Inet Reference
    new MythListButtonItem(m_inetrefList, QObject::tr("All"),
                           kInetRefFilterAll);
    new MythListButtonItem(m_inetrefList, QObject::tr("Unknown"),
                           kInetRefFilterUnknown);
    m_inetrefList->SetValueByData(m_settings.getInteRef());

    // Coverfile
    new MythListButtonItem(m_coverfileList, QObject::tr("All"),
                           kCoverFileFilterAll);
    new MythListButtonItem(m_coverfileList, QObject::tr("None"),
                           kCoverFileFilterNone);
    m_coverfileList->SetValueByData(m_settings.getCoverFile());

    // Order by
    new MythListButtonItem(m_orderbyList, QObject::tr("Title"),
                           VideoFilterSettings::kOrderByTitle);
    new MythListButtonItem(m_orderbyList, QObject::tr("Year"),
                           VideoFilterSettings::kOrderByYearDescending);
    new MythListButtonItem(m_orderbyList, QObject::tr("User Rating"),
                           VideoFilterSettings::kOrderByUserRatingDescending);
    new MythListButtonItem(m_orderbyList, QObject::tr("Runtime"),
                           VideoFilterSettings::kOrderByLength);
    new MythListButtonItem(m_orderbyList, QObject::tr("Filename"),
                           VideoFilterSettings::kOrderByFilename);
    new MythListButtonItem(m_orderbyList, QObject::tr("Video ID"),
                           VideoFilterSettings::kOrderByID);
    m_orderbyList->SetValueByData(m_settings.getOrderby());
}

void VideoFilterDialog::saveAsDefault()
{
     m_settings.saveAsDefault();
     saveAndExit();
}

void VideoFilterDialog::saveAndExit()
{
    if (m_fsp)
        m_fsp->setSettings(m_settings);
    if (m_settings.getChangedState() > 0)
        emit filterChanged();
    Close();
}

void VideoFilterDialog::setYear(MythListButtonItem *item)
{
    int new_year = item->GetData().toInt();
    m_settings.setYear(new_year);
    update_numvideo();
}

void VideoFilterDialog::setUserRating(MythListButtonItem *item)
{
    int new_userrating = item->GetData().toInt();
    m_settings.setUserrating(new_userrating);
    update_numvideo();
}

void VideoFilterDialog::setCategory(MythListButtonItem *item)
{
    int new_category = item->GetData().toInt();
    m_settings.setCategory(new_category);
    update_numvideo();
}

void VideoFilterDialog::setCountry(MythListButtonItem *item)
{
    int new_country = item->GetData().toInt();
    m_settings.setCountry(new_country);
    update_numvideo();
}

void VideoFilterDialog::setGenre(MythListButtonItem *item)
{
    int new_genre = item->GetData().toInt();
    m_settings.setGenre(new_genre);
    update_numvideo();
}

void VideoFilterDialog::setCast(MythListButtonItem *item)
{
    int new_cast = item->GetData().toInt();
    m_settings.setCast(new_cast);
    update_numvideo();
}

void VideoFilterDialog::setRunTime(MythListButtonItem *item)
{
    int new_runtime = item->GetData().toInt();
    m_settings.setRuntime(new_runtime);
    update_numvideo();
}

void VideoFilterDialog::setBrowse(MythListButtonItem *item)
{
    int new_browse = item->GetData().toInt();
    m_settings.setBrowse(new_browse);
    update_numvideo();
}

void VideoFilterDialog::setInetRef(MythListButtonItem *item)
{
    int new_inetref = item->GetData().toInt();
    m_settings.setInetRef(new_inetref);
    update_numvideo();
}

void VideoFilterDialog::setCoverFile(MythListButtonItem *item)
{
    int new_coverfile = item->GetData().toInt();
    m_settings.setCoverFile(new_coverfile);
    update_numvideo();
}

void VideoFilterDialog::setOrderby(MythListButtonItem *item)
{
    int new_orderby = item->GetData().toInt();
    m_settings.setOrderby(
            (VideoFilterSettings::ordering)new_orderby);
    update_numvideo();
}
