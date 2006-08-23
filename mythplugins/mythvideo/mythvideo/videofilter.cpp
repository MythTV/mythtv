/*
    videofilter.cpp
    (c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
    Part of the mythTV project
*/

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>

#include "globals.h"
#include "videofilter.h"
#include "videolist.h"
#include "dbaccess.h"
#include "metadata.h"
#include "metadatalistmanager.h"
#include "videoutils.h"

#include <set>

enum GenreFilter {
    kGenreFilterAll = -1,
    kGenreFilterUnknown = 0
};

enum CountryFilter {
    kCountryFilterAll = -1,
    kCountryFilterUnknown = 0
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
    country(kCountryFilterAll), year(kYearFilterAll),
    runtime(kRuntimeFilterAll), userrating(kUserRatingFilterAll),
    browse(kBrowseFilterAll), m_inetref(kInetRefFilterAll),
    m_coverfile(kCoverFileFilterAll), orderby(kOrderByTitle),
    m_parental_level(0), m_changed_state(0)
{
    if (!_prefix)
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
        bool found_genre = false;
        const Metadata::genre_list &gl = mdata.Genres();
        for (Metadata::genre_list::const_iterator p = gl.begin();
             p != gl.end(); ++p)
        {
            found_genre = p->first == genre;
        }
        matches = found_genre;
    }

    if (matches && country != kCountryFilterAll)
    {
        bool found_country = false;
        const Metadata::country_list &cl = mdata.Countries();
        for (Metadata::country_list::const_iterator p = cl.begin();
             p != cl.end(); ++p)
        {
            found_country = p->first == country;
        }
        matches = found_country;
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
        matches = (mdata.ShowLevel() != 0) &&
                (mdata.ShowLevel() <= m_parental_level);
    }

    return matches;
}

/// Compares two Metadata instances
bool VideoFilterSettings::meta_less_than(const Metadata &lhs,
                                         const Metadata &rhs) const
{
    bool ret = false;
    switch (orderby)
    {
        case kOrderByTitle:
        {
            QString lhs_key;
            QString rhs_key;
            if (lhs.hasSortKey() && rhs.hasSortKey())
            {
                lhs_key = lhs.getSortKey();
                rhs_key = rhs.getSortKey();
            }
            else
            {
                lhs_key = Metadata::GenerateDefaultSortKey(lhs);
                rhs_key = Metadata::GenerateDefaultSortKey(rhs);
            }
            ret = QString::localeAwareCompare(lhs_key, rhs_key) < 0;
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
            // TODO: honor case setting
            ret = QString::localeAwareCompare(lhs.Filename(),
                                              rhs.Filename()) < 0;
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
VideoFilterDialog::VideoFilterDialog(FilterSettingsProxy *fsp,
                                 MythMainWindow *parent_,
                                 QString window_name,
                                 QString theme_filename,
                                 const VideoList &video_list,
                                 const char *name_) :
    MythThemedDialog(parent_, window_name, theme_filename, name_),
    browse_select(0), orderby_select(0), year_select(0), userrating_select(0),
    category_select(0), country_select(0), genre_select(0), runtime_select(0),
    save_button(0), done_button(0), numvideos_text(0), m_intetref_select(0),
    m_coverfile_select(0), m_fsp(fsp), m_video_list(video_list)
{
    //
    //  The only thing this screen does is let the
    //  user set (some) metadata information. It only
    //  works on a single metadata entry.
    //

    m_settings = m_fsp->getSettings();

    wireUpTheme();
    fillWidgets();
    update_numvideo();
    assignFirstFocus();
}

void VideoFilterDialog::update_numvideo()
{
    if (numvideos_text)
    {
        int video_count = m_video_list.test_filter(m_settings);

        if (video_count > 0)
        {
            numvideos_text->SetText(
                    QString(tr("Result of this filter : %1 video(s)"))
                    .arg(video_count));
        }
        else
        {
            numvideos_text->SetText(
                    QString(tr("Result of this filter : No Videos")));
        }
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
            m_video_list.getListCache().getList();
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

    if (category_select)
    {
        category_select->addItem(kCategoryFilterAll, QObject::tr("All"));

        const VideoCategory::entry_list &vcl =
                VideoCategory::getCategory().getList();
        for (VideoCategory::entry_list::const_iterator p = vcl.begin();
             p != vcl.end(); ++p)
        {
            category_select->addItem(p->first, p->second);
        }

        category_select->addItem(kCategoryFilterUnknown,
                                 VIDEO_CATEGORY_UNKNOWN);
        category_select->setToItem(m_settings.getCategory());
    }

    if (genre_select)
    {
        genre_select->addItem(kGenreFilterAll, QObject::tr("All"));

        const VideoGenre::entry_list &gl = VideoGenre::getGenre().getList();
        for (VideoGenre::entry_list::const_iterator p = gl.begin();
             p != gl.end(); ++p)
        {
            genre_select->addItem(p->first, p->second);
        }

        genre_select->addItem(kGenreFilterUnknown, VIDEO_GENRE_UNKNOWN);
        genre_select->setToItem(m_settings.getGenre());
    }

    if (country_select)
    {
        country_select->addItem(kCountryFilterAll, QObject::tr("All"));

        const VideoCountry::entry_list &cl =
                VideoCountry::getCountry().getList();
        for (VideoCountry::entry_list::const_iterator p = cl.begin();
             p != cl.end(); ++p)
        {
            country_select->addItem(p->first, p->second);
        }

        country_select->addItem(kCountryFilterUnknown, VIDEO_COUNTRY_UNKNOWN);
        country_select->setToItem(m_settings.getCountry());
    }

    if (year_select)
    {
        year_select->addItem(kYearFilterAll, QObject::tr("All"));

        for (int_list::const_reverse_iterator p = years.rbegin();
             p != years.rend(); ++p)
        {
            year_select->addItem(*p, QString::number(*p));
        }

        if (have_unknown_year)
        {
            year_select->addItem(kYearFilterUnknown, VIDEO_YEAR_UNKNOWN);
        }

        year_select->setToItem(m_settings.getYear());
    }

    if (runtime_select)
    {
        runtime_select->addItem(kRuntimeFilterAll, QObject::tr("All"));

        if (have_unknown_runtime)
        {
            runtime_select->addItem(kRuntimeFilterUnknown,
                                    VIDEO_RUNTIME_UNKNOWN);
        }

        for (int_list::const_iterator p = runtimes.begin();
             p != runtimes.end(); ++p)
        {
            QString s = QString("%1 %2 ~ %3 %4").arg(*p * 30).arg(tr("minutes"))
                    .arg((*p + 1) * 30).arg(tr("minutes"));
            runtime_select->addItem(*p, s);
        }

        runtime_select->setToItem(m_settings.getRuntime());
    }

    if (userrating_select)
    {
        userrating_select->addItem(kUserRatingFilterAll, QObject::tr("All"));

        for (int_list::const_reverse_iterator p = user_ratings.rbegin();
             p != user_ratings.rend(); ++p)
        {
            userrating_select->addItem(*p, QString(">= %1")
                                       .arg(QString::number(*p)));
        }

        userrating_select->setToItem(m_settings.getUserrating());
    }

    if (browse_select)
    {
        browse_select->addItem(kBrowseFilterAll, QObject::tr("All"));
        browse_select->addItem(1, QObject::tr("Yes"));
        browse_select->addItem(0, QObject::tr("No"));
        browse_select->setToItem(m_settings.getBrowse());
    }

    if (m_intetref_select)
    {
        m_intetref_select->addItem(kInetRefFilterAll, QObject::tr("All"));
        m_intetref_select->addItem(kInetRefFilterUnknown,
                QObject::tr("Unknown"));
        m_intetref_select->setToItem(m_settings.getInteRef());
    }

    if (m_coverfile_select)
    {
        m_coverfile_select->addItem(kCoverFileFilterAll, QObject::tr("All"));
        m_coverfile_select->addItem(kCoverFileFilterNone,
                QObject::tr("None"));
        m_coverfile_select->setToItem(m_settings.getCoverFile());
    }

    if (orderby_select)
    {
        orderby_select->addItem(VideoFilterSettings::kOrderByTitle,
                                QObject::tr("Title"));
        orderby_select->addItem(VideoFilterSettings::kOrderByYearDescending,
                                QObject::tr("Year"));
        orderby_select->addItem(
                VideoFilterSettings::kOrderByUserRatingDescending,
                QObject::tr("User Rating"));
        orderby_select->addItem(VideoFilterSettings::kOrderByLength,
                                QObject::tr("Runtime"));
        orderby_select->addItem(VideoFilterSettings::kOrderByFilename,
                                QObject::tr("Filename"));
        orderby_select->addItem(VideoFilterSettings::kOrderByID,
                                QObject::tr("Video ID"));
        orderby_select->setToItem(m_settings.getOrderby());
    }
}

namespace
{
    void widget_testset(UISelectorType *&ret, UIType *current,
            UISelectorType *sel)
    {
        if (sel && current == sel) ret = sel;
    }
}

void VideoFilterDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool something_pushed = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if ((action == "LEFT") || (action == "RIGHT"))
        {
            something_pushed = false;

            UISelectorType *currentSelector = NULL;
            UIType *focued = getCurrentFocusWidget();
            widget_testset(currentSelector, focued, category_select);
            widget_testset(currentSelector, focued, genre_select);
            widget_testset(currentSelector, focued, country_select);
            widget_testset(currentSelector, focued, year_select);
            widget_testset(currentSelector, focued, runtime_select);
            widget_testset(currentSelector, focued, userrating_select);
            widget_testset(currentSelector, focued, browse_select);
            widget_testset(currentSelector, focued, m_intetref_select);
            widget_testset(currentSelector, focued, m_coverfile_select);
            widget_testset(currentSelector, focued, orderby_select);

            if (currentSelector)
            {
                currentSelector->push(action == "RIGHT");
                something_pushed = true;
            }

            if (!something_pushed)
            {
                activateCurrent();
            }
        }
        else if (action == "SELECT")
            activateCurrent();
        else if (action == "0")
        {
            if (done_button)
                done_button->push();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void VideoFilterDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);

    MythRemoteLineEdit *which_editor = (MythRemoteLineEdit *)sender();

    if (which_editor)
    {
        which_editor->clearFocus();
    }
}

void VideoFilterDialog::saveAsDefault()
{
     m_settings.saveAsDefault();
     this->saveAndExit();
}

void VideoFilterDialog::saveAndExit()
{
    m_fsp->setSettings(m_settings);
    done(0);
}

void VideoFilterDialog::setYear(int new_year)
{
        m_settings.setYear(new_year);
        update_numvideo();
}

void VideoFilterDialog::setUserRating(int new_userrating)
{
        m_settings.setUserrating(new_userrating);
        update_numvideo();
}

void VideoFilterDialog::setCategory(int new_category)
{
        m_settings.setCategory(new_category);
        update_numvideo();
}

void VideoFilterDialog::setCountry(int new_country)
{
        m_settings.setCountry(new_country);
        update_numvideo();
}

void VideoFilterDialog::setGenre(int new_genre)
{
        m_settings.setGenre(new_genre);
        update_numvideo();
}

void VideoFilterDialog::setRunTime(int new_runtime)
{
        m_settings.setRuntime(new_runtime);
        update_numvideo();
}

void VideoFilterDialog::setBrowse(int new_browse)
{
        m_settings.setBrowse(new_browse);
        update_numvideo();
}

void VideoFilterDialog::setInetRef(int new_inetref)
{
    m_settings.setInetRef(new_inetref);
    update_numvideo();
}

void VideoFilterDialog::setCoverFile(int new_coverfile)
{
    m_settings.setCoverFile(new_coverfile);
    update_numvideo();
}

void VideoFilterDialog::setOrderby(int new_orderby)
{
        m_settings.setOrderby(
                (VideoFilterSettings::ordering)new_orderby);
        update_numvideo();
}

void VideoFilterDialog::wireUpTheme()
{
    year_select = getUISelectorType("year_select");
    if (year_select)
        connect(year_select, SIGNAL(pushed(int)),
                this, SLOT(setYear(int)));

    userrating_select = getUISelectorType("userrating_select");
    if (userrating_select)
        connect(userrating_select, SIGNAL(pushed(int)),
                this, SLOT(setUserRating(int)));

    category_select = getUISelectorType("category_select");
    if (category_select)
        connect(category_select, SIGNAL(pushed(int)),
                this, SLOT(setCategory(int)));

    country_select = getUISelectorType("country_select");
    if (country_select)
        connect(country_select, SIGNAL(pushed(int)),
                this, SLOT(setCountry(int)));

    genre_select = getUISelectorType("genre_select");
    if (genre_select)
        connect(genre_select,SIGNAL(pushed(int)),
                this, SLOT(setGenre(int)));

    runtime_select = getUISelectorType("runtime_select");
    if (runtime_select)
        connect(runtime_select, SIGNAL(pushed(int)),
                this, SLOT(setRunTime(int)));

    browse_select = getUISelectorType("browse_select");
    if (browse_select)
        connect(browse_select, SIGNAL(pushed(int)),
                this, SLOT(setBrowse(int)));

    m_intetref_select = getUISelectorType("inetref_select");
    if (m_intetref_select)
        connect(m_intetref_select, SIGNAL(pushed(int)),
                this, SLOT(setInetRef(int)));

    m_coverfile_select = getUISelectorType("coverfile_select");
    if (m_coverfile_select)
        connect(m_coverfile_select, SIGNAL(pushed(int)),
                this, SLOT(setCoverFile(int)));

    orderby_select = getUISelectorType("orderby_select");
    if (orderby_select)
        connect(orderby_select, SIGNAL(pushed(int)),
                this, SLOT(setOrderby(int)));

    save_button = getUITextButtonType("save_button");

    if (save_button)
    {
        save_button->setText(tr("Save as default"));
        connect(save_button, SIGNAL(pushed()), this, SLOT(saveAsDefault()));
    }

    done_button = getUITextButtonType("done_button");
    if (done_button)
    {
        done_button->setText(tr("Done"));
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }

    numvideos_text = getUITextType("numvideos_text");
    buildFocusList();
}

VideoFilterDialog::~VideoFilterDialog()
{
}
