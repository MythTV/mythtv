// C++
#include <algorithm>

// Qt
#include <QImageReader>
#include <QUrl>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/stringutil.h"
#include "libmythmetadata/dbaccess.h"
#include "libmythmetadata/globals.h"
#include "libmythmetadata/metadatafactory.h"
#include "libmythmetadata/mythuiimageresults.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythmetadata/videoutils.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuicheckbox.h"
#include "libmythui/mythuifilebrowser.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuispinbox.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitextedit.h"

// MythFrontend
#include "editvideometadata.h"

//static const QString _Location = QObject::tr("Metadata Editor");

EditMetadataDialog::EditMetadataDialog(MythScreenStack *lparent,
        const QString& lname, VideoMetadata *source_metadata,
        const VideoMetadataListManager &cache) : MythScreenType(lparent, lname),
    m_origMetadata(source_metadata),
    m_metaCache(cache),
    m_query(new MetadataDownload(this)),
    m_imageDownload(new MetadataImageDownload(this))
{
    m_workingMetadata = new VideoMetadata(*m_origMetadata);
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

EditMetadataDialog::~EditMetadataDialog()
{
    delete m_workingMetadata;
}

bool EditMetadataDialog::Create()
{
    if (!LoadWindowFromXML("video-ui.xml", "edit_metadata", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_titleEdit, "title_edit", &err);
    UIUtilE::Assign(this, m_subtitleEdit, "subtitle_edit", &err);
    UIUtilE::Assign(this, m_playerEdit, "player_edit", &err);

    UIUtilE::Assign(this, m_seasonSpin, "season", &err);
    UIUtilE::Assign(this, m_episodeSpin, "episode", &err);


    UIUtilE::Assign(this, m_categoryList, "category_select", &err);
    UIUtilE::Assign(this, m_levelList, "level_select", &err);
    UIUtilE::Assign(this, m_childList, "child_select", &err);

    UIUtilE::Assign(this, m_browseCheck, "browse_check", &err);
    UIUtilE::Assign(this, m_watchedCheck, "watched_check", &err);

    UIUtilE::Assign(this, m_doneButton, "done_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'edit_metadata'");
        return false;
    }

    UIUtilW::Assign(this, m_coverartText, "coverart_text");
    UIUtilW::Assign(this, m_screenshotText, "screenshot_text");
    UIUtilW::Assign(this, m_bannerText, "banner_text");
    UIUtilW::Assign(this, m_fanartText, "fanart_text");
    UIUtilW::Assign(this, m_trailerText, "trailer_text");

    UIUtilW::Assign(this, m_coverartButton, "coverart_button");
    UIUtilW::Assign(this, m_bannerButton, "banner_button");
    UIUtilW::Assign(this, m_fanartButton, "fanart_button");
    UIUtilW::Assign(this, m_screenshotButton, "screenshot_button");
    UIUtilW::Assign(this, m_trailerButton, "trailer_button");

    UIUtilW::Assign(this, m_netBannerButton, "net_banner_button");
    UIUtilW::Assign(this, m_netFanartButton, "net_fanart_button");
    UIUtilW::Assign(this, m_netScreenshotButton, "net_screenshot_button");
    UIUtilW::Assign(this, m_netCoverartButton, "net_coverart_button");

    UIUtilW::Assign(this, m_taglineEdit, "tagline_edit");
    UIUtilW::Assign(this, m_ratingEdit, "rating_edit");
    UIUtilW::Assign(this, m_directorEdit, "director_edit");
    UIUtilW::Assign(this, m_inetrefEdit, "inetref_edit");
    UIUtilW::Assign(this, m_homepageEdit, "homepage_edit");
    UIUtilW::Assign(this, m_plotEdit, "description_edit");
    UIUtilW::Assign(this, m_yearSpin, "year_spin");
    UIUtilW::Assign(this, m_userRatingSpin, "userrating_spin");
    UIUtilW::Assign(this, m_lengthSpin, "length_spin");

    UIUtilW::Assign(this, m_coverart, "coverart");
    UIUtilW::Assign(this, m_screenshot, "screenshot");
    UIUtilW::Assign(this, m_banner, "banner");
    UIUtilW::Assign(this, m_fanart, "fanart");

    fillWidgets();

    BuildFocusList();

    connect(m_titleEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetTitle);
    m_titleEdit->SetMaxLength(128);
    connect(m_subtitleEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetSubtitle);
    m_subtitleEdit->SetMaxLength(0);
    connect(m_playerEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetPlayer);
    if (m_taglineEdit)
    {
        connect(m_taglineEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetTagline);
        m_taglineEdit->SetMaxLength(255);
    }
    if (m_ratingEdit)
    {
        connect(m_ratingEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetRating);
        m_ratingEdit->SetMaxLength(128);
    }
    if (m_directorEdit)
    {
        connect(m_directorEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetDirector);
        m_directorEdit->SetMaxLength(128);
    }
    if (m_inetrefEdit)
        connect(m_inetrefEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetInetRef);
    if (m_homepageEdit)
    {
        connect(m_homepageEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetHomepage);
        m_homepageEdit->SetMaxLength(0);
    }
    if (m_plotEdit)
    {
        connect(m_plotEdit, &MythUITextEdit::valueChanged, this, &EditMetadataDialog::SetPlot);
        m_plotEdit->SetMaxLength(0);
    }

    connect(m_seasonSpin, &MythUIType::LosingFocus, this, &EditMetadataDialog::SetSeason);
    connect(m_episodeSpin, &MythUIType::LosingFocus, this, &EditMetadataDialog::SetEpisode);
    if (m_yearSpin)
        connect(m_yearSpin, &MythUIType::LosingFocus, this, &EditMetadataDialog::SetYear);
    if (m_userRatingSpin)
        connect(m_userRatingSpin, &MythUIType::LosingFocus, this, &EditMetadataDialog::SetUserRating);
    if (m_lengthSpin)
        connect(m_lengthSpin, &MythUIType::LosingFocus, this, &EditMetadataDialog::SetLength);

    connect(m_doneButton, &MythUIButton::Clicked, this, &EditMetadataDialog::SaveAndExit);

    // Find Artwork locally
    if (m_coverartButton)
        connect(m_coverartButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindCoverArt);
    if (m_bannerButton)
        connect(m_bannerButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindBanner);
    if (m_fanartButton)
        connect(m_fanartButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindFanart);
    if (m_screenshotButton)
        connect(m_screenshotButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindScreenshot);

    // Find Artwork on the Internet
    if (m_netCoverartButton)
        connect(m_netCoverartButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindNetCoverArt);
    if (m_netBannerButton)
        connect(m_netBannerButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindNetBanner);
    if (m_netFanartButton)
        connect(m_netFanartButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindNetFanart);
    if (m_netScreenshotButton)
        connect(m_netScreenshotButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindNetScreenshot);

    if (m_trailerButton)
        connect(m_trailerButton, &MythUIButton::Clicked, this, &EditMetadataDialog::FindTrailer);

    connect(m_browseCheck, &MythUICheckBox::valueChanged, this, &EditMetadataDialog::ToggleBrowse);
    connect(m_watchedCheck, &MythUICheckBox::valueChanged, this, &EditMetadataDialog::ToggleWatched);

    connect(m_childList, &MythUIButtonList::itemSelected,
            this, &EditMetadataDialog::SetChild);
    connect(m_levelList, &MythUIButtonList::itemSelected,
            this, &EditMetadataDialog::SetLevel);
    connect(m_categoryList, &MythUIButtonList::itemSelected,
            this, &EditMetadataDialog::SetCategory);
    connect(m_categoryList, &MythUIButtonList::itemClicked,
            this, &EditMetadataDialog::NewCategoryPopup);

    return true;
}

namespace
{
    template <typename T>
    struct title_sort
    {
        bool operator()(const T &lhs, const T &rhs)
        {
            return StringUtil::naturalSortCompare(lhs.second, rhs.second);
        }
    };

    QStringList GetSupportedImageExtensionFilter()
    {
        QStringList ret;

        QList<QByteArray> exts = QImageReader::supportedImageFormats();
        for (const auto & ext : std::as_const(exts))
            ret.append(QString("*.").append(ext));
        return ret;
    }

    void FindImagePopup(const QString &prefix, const QString &prefixAlt,
            QObject &inst, const QString &returnEvent)
    {
        QString fp;

        if (prefix.startsWith("myth://"))
            fp = prefix;
        else
            fp = prefix.isEmpty() ? prefixAlt : prefix;

        MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");

        auto *fb = new MythUIFileBrowser(popupStack, fp);
        fb->SetNameFilter(GetSupportedImageExtensionFilter());
        if (fb->Create())
        {
            fb->SetReturnEvent(&inst, returnEvent);
            popupStack->AddScreen(fb);
        }
        else
        {
            delete fb;
        }
    }

    void FindVideoFilePopup(const QString &prefix, const QString &prefixAlt,
            QObject &inst, const QString &returnEvent)
    {
        QString fp;

        if (prefix.startsWith("myth://"))
            fp = prefix;
        else
            fp = prefix.isEmpty() ? prefixAlt : prefix;

        MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
        QStringList exts;

        const FileAssociations::association_list fa_list =
                FileAssociations::getFileAssociation().getList();
        for (const auto & fa : fa_list)
            exts << QString("*.%1").arg(fa.extension.toUpper());

        auto *fb = new MythUIFileBrowser(popupStack, fp);
        fb->SetNameFilter(exts);
        if (fb->Create())
        {
            fb->SetReturnEvent(&inst, returnEvent);
            popupStack->AddScreen(fb);
        }
        else
        {
            delete fb;
        }
    }

    const QString CEID_COVERARTFILE = "coverartfile";
    const QString CEID_BANNERFILE = "bannerfile";
    const QString CEID_FANARTFILE = "fanartfile";
    const QString CEID_SCREENSHOTFILE = "screenshotfile";
    const QString CEID_TRAILERFILE = "trailerfile";
    const QString CEID_NEWCATEGORY = "newcategory";
}

void EditMetadataDialog::createBusyDialog(const QString& title)
{
    if (m_busyPopup)
        return;

    const QString& message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

void EditMetadataDialog::fillWidgets()
{
    m_titleEdit->SetText(m_workingMetadata->GetTitle());
    m_subtitleEdit->SetText(m_workingMetadata->GetSubtitle());

    m_seasonSpin->SetRange(0,9999,1,5);
    m_seasonSpin->SetValue(m_workingMetadata->GetSeason());
    m_episodeSpin->SetRange(0,999,1,10);
    m_episodeSpin->SetValue(m_workingMetadata->GetEpisode());
    if (m_yearSpin)
    {
        m_yearSpin->SetRange(0,9999,1,100);
        m_yearSpin->SetValue(m_workingMetadata->GetYear());
    }
    if (m_userRatingSpin)
    {
        m_userRatingSpin->SetRange(0,10,1,2);
        m_userRatingSpin->SetValue(m_workingMetadata->GetUserRating());
    }
    if (m_lengthSpin)
    {
        m_lengthSpin->SetRange(0,999,1,15);
        m_lengthSpin->SetValue(m_workingMetadata->GetLength().count());
    }

    // No memory leak. MythUIButtonListItem adds the new item into
    // m_categoryList.
    new MythUIButtonListItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN);
    const VideoCategory::entry_list &vcl =
            VideoCategory::GetCategory().getList();
    for (const auto & vc : vcl)
    {
        // No memory leak. MythUIButtonListItem adds the new item into
        // m_categoryList.
        auto *button = new MythUIButtonListItem(m_categoryList, vc.second);
        button->SetData(vc.first);
    }
    m_categoryList->SetValueByData(m_workingMetadata->GetCategoryID());

    for (ParentalLevel i = ParentalLevel::plLowest;
            i <= ParentalLevel::plHigh && i.good(); ++i)
    {
        // No memory leak. MythUIButtonListItem adds the new item into
        // m_levelList.
        auto *button = new MythUIButtonListItem(m_levelList,
                                          tr("Level %1").arg(i.GetLevel()));
        button->SetData(i.GetLevel());
    }
    m_levelList->SetValueByData(m_workingMetadata->GetShowLevel());

    //
    //  Fill the "always play this video next" option
    //  with all available videos.
    //

    // No memory leak. MythUIButtonListItem adds the new item into
    // m_childList.
    new MythUIButtonListItem(m_childList,tr("None"));

    // TODO: maybe make the title list have the same sort order
    // as elsewhere.
    using title_list = std::vector<std::pair<unsigned int, QString> >;
    const VideoMetadataListManager::metadata_list &mdl = m_metaCache.getList();
    title_list tc;
    tc.reserve(mdl.size());
    for (const auto & md : mdl)
    {
        QString title;
        if (md->GetSeason() > 0 || md->GetEpisode() > 0)
        {
            title = QString("%1 %2x%3").arg(md->GetTitle(),
                                            QString::number(md->GetSeason()),
                                            QString::number(md->GetEpisode()));
        }
        else
        {
            title = md->GetTitle();
        }
        tc.emplace_back(md->GetID(), title);
    }
    std::sort(tc.begin(), tc.end(), title_sort<title_list::value_type>());

    for (const auto & t : tc)
    {
        if (t.first != m_workingMetadata->GetID())
        {
            auto *button = new MythUIButtonListItem(m_childList,t.second);
            button->SetData(t.first);
        }
    }

    if (m_workingMetadata->GetChildID() > 0)
    {
        m_childList->SetValueByData(m_workingMetadata->GetChildID());
        m_cachedChildSelection = m_workingMetadata->GetChildID();
    }

    if (m_workingMetadata->GetBrowse())
        m_browseCheck->SetCheckState(MythUIStateType::Full);
    if (m_workingMetadata->GetWatched())
        m_watchedCheck->SetCheckState(MythUIStateType::Full);
    if (m_coverartText)
        m_coverartText->SetText(m_workingMetadata->GetCoverFile());
    if (m_screenshotText)
        m_screenshotText->SetText(m_workingMetadata->GetScreenshot());
    if (m_bannerText)
        m_bannerText->SetText(m_workingMetadata->GetBanner());
    if (m_fanartText)
        m_fanartText->SetText(m_workingMetadata->GetFanart());
    if (m_trailerText)
        m_trailerText->SetText(m_workingMetadata->GetTrailer());

    m_playerEdit->SetText(m_workingMetadata->GetPlayCommand());
    if (m_taglineEdit)
        m_taglineEdit->SetText(m_workingMetadata->GetTagline());
    if (m_ratingEdit)
        m_ratingEdit->SetText(m_workingMetadata->GetRating());
    if (m_directorEdit)
        m_directorEdit->SetText(m_workingMetadata->GetDirector());
    if (m_inetrefEdit)
        m_inetrefEdit->SetText(m_workingMetadata->GetInetRef());
    if (m_homepageEdit)
        m_homepageEdit->SetText(m_workingMetadata->GetHomepage());
    if (m_plotEdit)
        m_plotEdit->SetText(m_workingMetadata->GetPlot());

    if (m_coverart)
    {
        if (!m_workingMetadata->GetHost().isEmpty() &&
            !m_workingMetadata->GetCoverFile().isEmpty() &&
            !m_workingMetadata->GetCoverFile().startsWith("/"))
        {
            m_coverart->SetFilename(generate_file_url("Coverart",
                                  m_workingMetadata->GetHost(),
                                  m_workingMetadata->GetCoverFile()));
        }
        else
        {
            m_coverart->SetFilename(m_workingMetadata->GetCoverFile());
        }

        if (!m_workingMetadata->GetCoverFile().isEmpty())
            m_coverart->Load();
    }
    if (m_screenshot)
    {
        if (!m_workingMetadata->GetHost().isEmpty() &&
            !m_workingMetadata->GetScreenshot().isEmpty() &&
            !m_workingMetadata->GetScreenshot().startsWith("/"))
        {
            m_screenshot->SetFilename(generate_file_url("Screenshots",
                                  m_workingMetadata->GetHost(),
                                  m_workingMetadata->GetScreenshot()));
        }
        else
        {
            m_screenshot->SetFilename(m_workingMetadata->GetScreenshot());
        }

        if (!m_workingMetadata->GetScreenshot().isEmpty())
            m_screenshot->Load();
    }
    if (m_banner)
    {
        if (!m_workingMetadata->GetHost().isEmpty() &&
            !m_workingMetadata->GetBanner().isEmpty() &&
            !m_workingMetadata->GetBanner().startsWith("/"))
        {
            m_banner->SetFilename(generate_file_url("Banners",
                                  m_workingMetadata->GetHost(),
                                  m_workingMetadata->GetBanner()));
        }
        else
        {
            m_banner->SetFilename(m_workingMetadata->GetBanner());
        }

        if (!m_workingMetadata->GetBanner().isEmpty())
            m_banner->Load();
    }
    if (m_fanart)
    {
        if (!m_workingMetadata->GetHost().isEmpty() &&
            !m_workingMetadata->GetFanart().isEmpty() &&
            !m_workingMetadata->GetFanart().startsWith("/"))
        {
            m_fanart->SetFilename(generate_file_url("Fanart",
                                  m_workingMetadata->GetHost(),
                                  m_workingMetadata->GetFanart()));
        }
        else
        {
            m_fanart->SetFilename(m_workingMetadata->GetFanart());
        }

        if (!m_workingMetadata->GetFanart().isEmpty())
            m_fanart->Load();
    }
}

void EditMetadataDialog::NewCategoryPopup()
{
    QString message = tr("Enter new category");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *categorydialog = new MythTextInputDialog(popupStack,message);

    if (categorydialog->Create())
    {
        categorydialog->SetReturnEvent(this, CEID_NEWCATEGORY);
        popupStack->AddScreen(categorydialog);
    }

}

void EditMetadataDialog::AddCategory(const QString& category)
{
    int id = VideoCategory::GetCategory().add(category);
    m_workingMetadata->SetCategoryID(id);
    new MythUIButtonListItem(m_categoryList, category, id);
    m_categoryList->SetValueByData(id);
}

void EditMetadataDialog::SaveAndExit()
{
    *m_origMetadata = *m_workingMetadata;
    m_origMetadata->UpdateDatabase();

    emit Finished();
    Close();
}

void EditMetadataDialog::SetTitle()
{
    m_workingMetadata->SetTitle(m_titleEdit->GetText());
}

void EditMetadataDialog::SetSubtitle()
{
    m_workingMetadata->SetSubtitle(m_subtitleEdit->GetText());
}

void EditMetadataDialog::SetCategory(MythUIButtonListItem *item)
{
    m_workingMetadata->SetCategoryID(item->GetData().toInt());
}

void EditMetadataDialog::SetRating()
{
    m_workingMetadata->SetRating(m_ratingEdit->GetText());
}

void EditMetadataDialog::SetTagline()
{
    m_workingMetadata->SetTagline(m_taglineEdit->GetText());
}

void EditMetadataDialog::SetDirector()
{
    m_workingMetadata->SetDirector(m_directorEdit->GetText());
}

void EditMetadataDialog::SetInetRef()
{
    m_workingMetadata->SetInetRef(m_inetrefEdit->GetText());
}

void EditMetadataDialog::SetHomepage()
{
    m_workingMetadata->SetHomepage(m_homepageEdit->GetText());
}

void EditMetadataDialog::SetPlot()
{
    m_workingMetadata->SetPlot(m_plotEdit->GetText());
}

void EditMetadataDialog::SetSeason()
{
    m_workingMetadata->SetSeason(m_seasonSpin->GetIntValue());
}

void EditMetadataDialog::SetEpisode()
{
    m_workingMetadata->SetEpisode(m_episodeSpin->GetIntValue());
}

void EditMetadataDialog::SetYear()
{
    m_workingMetadata->SetYear(m_yearSpin->GetIntValue());
}

void EditMetadataDialog::SetUserRating()
{
    m_workingMetadata->SetUserRating(m_userRatingSpin->GetIntValue());
}

void EditMetadataDialog::SetLength()
{
    m_workingMetadata->SetLength(m_lengthSpin->GetDuration<std::chrono::minutes>());
}

void EditMetadataDialog::SetPlayer()
{
    m_workingMetadata->SetPlayCommand(m_playerEdit->GetText());
}

void EditMetadataDialog::SetLevel(MythUIButtonListItem *item)
{
    m_workingMetadata->
            SetShowLevel(ParentalLevel(item->GetData().toInt()).GetLevel());
}

void EditMetadataDialog::SetChild(MythUIButtonListItem *item)
{
    m_cachedChildSelection = item->GetData().toInt();
    m_workingMetadata->SetChildID(m_cachedChildSelection);
}

void EditMetadataDialog::ToggleBrowse()
{
    m_workingMetadata->
            SetBrowse(m_browseCheck->GetBooleanCheckState());
}

void EditMetadataDialog::ToggleWatched()
{
    m_workingMetadata->
            SetWatched(m_watchedCheck->GetBooleanCheckState());
}

void EditMetadataDialog::FindCoverArt()
{
    if (!m_workingMetadata->GetHost().isEmpty())
    {
        QString url = generate_file_url("Coverart",
                      m_workingMetadata->GetHost(),
                      "");
        FindImagePopup(url, "", *this, CEID_COVERARTFILE);
    }
    else
    {
        FindImagePopup(gCoreContext->GetSetting("VideoArtworkDir"),
                GetConfDir() + "/MythVideo", *this, CEID_COVERARTFILE);
    }
}

void EditMetadataDialog::OnArtworkSearchDone(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = nullptr;
    }

    auto type = lookup->GetData().value<VideoArtworkType>();
    ArtworkList list = lookup->GetArtwork(type);

    if (list.isEmpty())
    {
        MythWarningNotification n(tr("No image found"), tr("Metadata Editor"));
        GetNotificationCenter()->Queue(n);
        return;
    }
    auto *resultsdialog = new ImageSearchResultsDialog(m_popupStack, list, type);

    connect(resultsdialog, &ImageSearchResultsDialog::haveResult,
            this, &EditMetadataDialog::OnSearchListSelection);

    if (resultsdialog->Create())
        m_popupStack->AddScreen(resultsdialog);
}

void EditMetadataDialog::OnSearchListSelection(const ArtworkInfo& info, VideoArtworkType type)
{
    QString msg = tr("Downloading selected artwork...");
    createBusyDialog(msg);

    auto *lookup = new MetadataLookup();
    lookup->SetType(kMetadataVideo);

    lookup->SetSubtype(GuessLookupType(m_workingMetadata));
    lookup->SetHost(m_workingMetadata->GetHost());
    lookup->SetAutomatic(true);
    lookup->SetData(QVariant::fromValue<VideoArtworkType>(type));

    DownloadMap downloads;
    downloads.insert(type, info);
    lookup->SetDownloads(downloads);
    lookup->SetAllowOverwrites(true);
    lookup->SetTitle(m_workingMetadata->GetTitle());
    lookup->SetSubtitle(m_workingMetadata->GetSubtitle());
    lookup->SetSeason(m_workingMetadata->GetSeason());
    lookup->SetEpisode(m_workingMetadata->GetEpisode());
    lookup->SetInetref(m_workingMetadata->GetInetRef());

    m_imageDownload->addDownloads(lookup);
}

void EditMetadataDialog::handleDownloadedImages(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = nullptr;
    }

    auto type = lookup->GetData().value<VideoArtworkType>();
    DownloadMap map = lookup->GetDownloads();

    if (map.count() >= 1)
    {
        ArtworkInfo info = map.value(type);
        QString filename = info.url;

        if (type == kArtworkCoverart)
            SetCoverArt(filename);
        else if (type == kArtworkFanart)
            SetFanart(filename);
        else if (type == kArtworkBanner)
            SetBanner(filename);
        else if (type == kArtworkScreenshot)
            SetScreenshot(filename);
    }
}

void EditMetadataDialog::FindNetArt(VideoArtworkType type)
{
    QString msg = tr("Searching for available artwork...");
    createBusyDialog(msg);

    auto *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataVideo);
    lookup->SetAutomatic(true);
    if (m_workingMetadata->GetSeason() > 0 ||
            m_workingMetadata->GetEpisode() > 0)
        lookup->SetSubtype(kProbableTelevision);
    else if (m_workingMetadata->GetSubtitle().isEmpty())
        lookup->SetSubtype(kProbableMovie);
    else
        lookup->SetSubtype(kUnknownVideo);
    lookup->SetData(QVariant::fromValue<VideoArtworkType>(type));

    lookup->SetTitle(m_workingMetadata->GetTitle());
    lookup->SetSubtitle(m_workingMetadata->GetSubtitle());
    lookup->SetSeason(m_workingMetadata->GetSeason());
    lookup->SetEpisode(m_workingMetadata->GetEpisode());
    lookup->SetInetref(m_workingMetadata->GetInetRef());

    m_query->addLookup(lookup);
}

void EditMetadataDialog::FindNetCoverArt()
{
    FindNetArt(kArtworkCoverart);
}

void EditMetadataDialog::FindNetFanart()
{
    FindNetArt(kArtworkFanart);
}

void EditMetadataDialog::FindNetBanner()
{
    FindNetArt(kArtworkBanner);
}

void EditMetadataDialog::FindNetScreenshot()
{
    FindNetArt(kArtworkScreenshot);
}

void EditMetadataDialog::SetCoverArt(QString file)
{
    if (file.isEmpty())
        return;

    QString origfile = file;

    if (file.startsWith("myth://"))
    {
        QUrl url(file);
        file = url.path();
        file = file.right(file.length() - 1);
        if (!file.endsWith("/"))
            m_workingMetadata->SetCoverFile(file);
        else
            m_workingMetadata->SetCoverFile(QString());
    }
    else
    {
        m_workingMetadata->SetCoverFile(file);
    }

    CheckedSet(m_coverartText, file);

    if (m_coverart)
    {
        m_coverart->SetFilename(origfile);
        m_coverart->Load();
    }
}

void EditMetadataDialog::FindBanner()
{
    if (!m_workingMetadata->GetHost().isEmpty())
    {
        QString url = generate_file_url("Banners",
                      m_workingMetadata->GetHost(),
                      "");
        FindImagePopup(url, "", *this, CEID_BANNERFILE);
    }
    else
    {
        FindImagePopup(gCoreContext->GetSetting("mythvideo.bannerDir"),
                GetConfDir() + "/MythVideo/Banners", *this, CEID_BANNERFILE);
    }
}

void EditMetadataDialog::SetBanner(QString file)
{
    if (file.isEmpty())
        return;

    QString origfile = file;

    if (file.startsWith("myth://"))
    {
        QUrl url(file);
        file = url.path();
        file = file.right(file.length() - 1);
        if (!file.endsWith("/"))
            m_workingMetadata->SetBanner(file);
        else
            m_workingMetadata->SetBanner(QString());
    }
    else
    {
        m_workingMetadata->SetBanner(file);
    }

    CheckedSet(m_bannerText, file);

    if (m_banner)
    {
        m_banner->SetFilename(origfile);
        m_banner->Load();
    }
}

void EditMetadataDialog::FindFanart()
{
    if (!m_workingMetadata->GetHost().isEmpty())
    {
        QString url = generate_file_url("Fanart",
                      m_workingMetadata->GetHost(),
                      "");
        FindImagePopup(url, "", *this, CEID_FANARTFILE);
    }
    else
    {
        FindImagePopup(gCoreContext->GetSetting("mythvideo.fanartDir"),
                GetConfDir() + "/MythVideo/Fanart", *this, CEID_FANARTFILE);
    }
}

void EditMetadataDialog::SetFanart(QString file)
{
    if (file.isEmpty())
        return;

    QString origfile = file;

    if (file.startsWith("myth://"))
    {
        QUrl url(file);
        file = url.path();
        file = file.right(file.length() - 1);
        if (!file.endsWith("/"))
            m_workingMetadata->SetFanart(file);
        else
            m_workingMetadata->SetFanart(QString());
    }
    else
    {
        m_workingMetadata->SetFanart(file);
    }

    CheckedSet(m_fanartText, file);

    if (m_fanart)
    {
        m_fanart->SetFilename(origfile);
        m_fanart->Load();
    }
}

void EditMetadataDialog::FindScreenshot()
{
    if (!m_workingMetadata->GetHost().isEmpty())
    {
        QString url = generate_file_url("Screenshots",
                      m_workingMetadata->GetHost(),
                      "");
        FindImagePopup(url, "", *this, CEID_SCREENSHOTFILE);
    }
    else
    {
        FindImagePopup(gCoreContext->GetSetting("mythvideo.screenshotDir"),
                GetConfDir() + "/MythVideo/Screenshots",
                *this, CEID_SCREENSHOTFILE);
    }
}

void EditMetadataDialog::SetScreenshot(QString file)
{
    if (file.isEmpty())
        return;

    QString origfile = file;

    if (file.startsWith("myth://"))
    {
        QUrl url(file);
        file = url.path();
        file = file.right(file.length() - 1);
        if (!file.endsWith("/"))
            m_workingMetadata->SetScreenshot(file);
        else
            m_workingMetadata->SetScreenshot(QString());
    }
    else
    {
        m_workingMetadata->SetScreenshot(file);
    }

    CheckedSet(m_screenshotText, file);

    if (m_screenshot)
    {
        m_screenshot->SetFilename(origfile);
        m_screenshot->Load();
    }
}

void EditMetadataDialog::FindTrailer()
{
    if (!m_workingMetadata->GetHost().isEmpty())
    {
        QString url = generate_file_url("Trailers",
                      m_workingMetadata->GetHost(),
                      "");
        FindVideoFilePopup(url, "", *this, CEID_TRAILERFILE);
    }
    else
    {
        FindVideoFilePopup(gCoreContext->GetSetting("mythvideo.TrailersDir"),
                GetConfDir() + "/MythVideo/Trailers", *this, CEID_TRAILERFILE);
    }
}

void EditMetadataDialog::SetTrailer(QString file)
{
    if (file.isEmpty())
        return;

    if (file.startsWith("myth://"))
    {
        QUrl url(file);
        file = url.path();
        file = file.right(file.length() - 1);
        if (!file.endsWith("/"))
            m_workingMetadata->SetTrailer(file);
        else
            m_workingMetadata->SetTrailer(QString());
    }
    else
    {
        m_workingMetadata->SetTrailer(file);
    }
    CheckedSet(m_trailerText, file);
}

void EditMetadataDialog::customEvent(QEvent *levent)
{
    if (levent->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(levent);

        const QString resultid = dce->GetId();

        if (resultid == CEID_COVERARTFILE)
            SetCoverArt(dce->GetResultText());
        else if (resultid == CEID_BANNERFILE)
            SetBanner(dce->GetResultText());
        else if (resultid == CEID_FANARTFILE)
            SetFanart(dce->GetResultText());
        else if (resultid == CEID_SCREENSHOTFILE)
            SetScreenshot(dce->GetResultText());
        else if (resultid == CEID_TRAILERFILE)
            SetTrailer(dce->GetResultText());
        else if (resultid == CEID_NEWCATEGORY)
            AddCategory(dce->GetResultText());
    }
    else if (levent->type() == MetadataLookupEvent::kEventType)
    {
        auto *lue = (MetadataLookupEvent *)levent;

        MetadataLookupList lul = lue->m_lookupList;

        if (lul.isEmpty())
            return;

        // There should really only be one result here.
        // If not, USER ERROR!
        if (lul.count() == 1)
        {
            OnArtworkSearchDone(lul[0]);
        }
        else
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = nullptr;
            }
        }
    }
    else if (levent->type() == MetadataLookupFailure::kEventType)
    {
        auto *luf = (MetadataLookupFailure *)levent;

        MetadataLookupList lul = luf->m_lookupList;

        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        if (!lul.empty())
        {
            MetadataLookup *lookup = lul[0];
            LOG(VB_GENERAL, LOG_INFO,
                QString("No results found for %1 %2 %3").arg(lookup->GetTitle())
                    .arg(lookup->GetSeason()).arg(lookup->GetEpisode()));
        }
    }
    else if (levent->type() == ImageDLEvent::kEventType)
    {
        auto *ide = (ImageDLEvent *)levent;

        MetadataLookup *lookup = ide->m_item;

        if (!lookup)
            return;

        handleDownloadedImages(lookup);
    }
    else if (levent->type() == ImageDLFailureEvent::kEventType)
    {
        MythErrorNotification n(tr("Failed to retrieve image"),
                                tr("Metadata Editor"),
                                tr("Check logs"));
        GetNotificationCenter()->Queue(n);
    }
}
