#include <algorithm>

#include <QImageReader>
#include <QUrl>

#include <mythcontext.h>
#include <mythdirs.h>

#include "mythmainwindow.h"
#include "mythdialogbox.h"
#include "mythuibuttonlist.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "mythuibutton.h"
#include "mythuicheckbox.h"
#include "mythuispinbox.h"
#include "mythuifilebrowser.h"
#include "mythuiimageresults.h"
#include "mythuihelper.h"
#include "mythprogressdialog.h"
#include "remoteutil.h"
#include "globals.h"
#include "dbaccess.h"
#include "videometadatalistmanager.h"
#include "videoutils.h"

#include "editvideometadata.h"

EditMetadataDialog::EditMetadataDialog(MythScreenStack *lparent,
        QString lname, VideoMetadata *source_metadata,
        const VideoMetadataListManager &cache) : MythScreenType(lparent, lname),
    m_origMetadata(source_metadata), m_titleEdit(0), m_subtitleEdit(0),
    m_taglineEdit(0), m_playerEdit(0), m_ratingEdit(0), m_directorEdit(0),
    m_inetrefEdit(0), m_homepageEdit(0), m_plotEdit(0), m_seasonSpin(0),
    m_episodeSpin(0), m_yearSpin(0), m_userRatingSpin(0), m_lengthSpin(0),
    m_categoryList(0), m_levelList(0), m_childList(0), m_browseCheck(0),
    m_watchedCheck(0), m_coverartButton(0), m_coverartText(0),
    m_screenshotButton(0), m_screenshotText(0), m_bannerButton(0),
    m_bannerText(0), m_fanartButton(0), m_fanartText(0),
    m_trailerButton(0), m_trailerText(0),
    m_netCoverartButton(0), m_netFanartButton(0), m_netBannerButton(0),
    m_netScreenshotButton(0), m_coverart(0), m_screenshot(0),
    m_banner(0), m_fanart(0),
    m_doneButton(0),
    cachedChildSelection(0),
    m_metaCache(cache), m_busyPopup(0)
{
    m_query = new MetadataDownload(this);
    m_imageDownload = new MetadataImageDownload(this);
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

    connect(m_titleEdit, SIGNAL(valueChanged()), SLOT(SetTitle()));
    m_titleEdit->SetMaxLength(128);
    connect(m_subtitleEdit, SIGNAL(valueChanged()), SLOT(SetSubtitle()));
    m_subtitleEdit->SetMaxLength(0);
    connect(m_playerEdit, SIGNAL(valueChanged()), SLOT(SetPlayer()));
    if (m_taglineEdit)
    {
        connect(m_taglineEdit, SIGNAL(valueChanged()), SLOT(SetTagline()));
        m_taglineEdit->SetMaxLength(255);
    }
    if (m_ratingEdit)
    {
        connect(m_ratingEdit, SIGNAL(valueChanged()), SLOT(SetRating()));
        m_ratingEdit->SetMaxLength(128);
    }
    if (m_directorEdit)
    {
        connect(m_directorEdit, SIGNAL(valueChanged()), SLOT(SetDirector()));
        m_directorEdit->SetMaxLength(128);
    }
    if (m_inetrefEdit)
        connect(m_inetrefEdit, SIGNAL(valueChanged()), SLOT(SetInetRef()));
    if (m_homepageEdit)
    {
        connect(m_homepageEdit, SIGNAL(valueChanged()), SLOT(SetHomepage()));
        m_homepageEdit->SetMaxLength(0);
    }
    if (m_plotEdit)
    {
        connect(m_plotEdit, SIGNAL(valueChanged()), SLOT(SetPlot()));
        m_plotEdit->SetMaxLength(0);
    }

    connect(m_seasonSpin, SIGNAL(LosingFocus()), SLOT(SetSeason()));
    connect(m_episodeSpin, SIGNAL(LosingFocus()), SLOT(SetEpisode()));
    if (m_yearSpin)
        connect(m_yearSpin, SIGNAL(LosingFocus()), SLOT(SetYear()));
    if (m_userRatingSpin)
        connect(m_userRatingSpin, SIGNAL(LosingFocus()), SLOT(SetUserRating()));
    if (m_lengthSpin)
        connect(m_lengthSpin, SIGNAL(LosingFocus()), SLOT(SetLength()));

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(SaveAndExit()));

    // Find Artwork locally
    if (m_coverartButton)
        connect(m_coverartButton, SIGNAL(Clicked()), SLOT(FindCoverArt()));
    if (m_bannerButton)
        connect(m_bannerButton, SIGNAL(Clicked()), SLOT(FindBanner()));
    if (m_fanartButton)
        connect(m_fanartButton, SIGNAL(Clicked()), SLOT(FindFanart()));
    if (m_screenshotButton)
        connect(m_screenshotButton, SIGNAL(Clicked()), SLOT(FindScreenshot()));

    // Find Artwork on the Internet
    if (m_netCoverartButton)
        connect(m_netCoverartButton, SIGNAL(Clicked()), SLOT(FindNetCoverArt()));
    if (m_netBannerButton)
        connect(m_netBannerButton, SIGNAL(Clicked()), SLOT(FindNetBanner()));
    if (m_netFanartButton)
        connect(m_netFanartButton, SIGNAL(Clicked()), SLOT(FindNetFanart()));
    if (m_netScreenshotButton)
        connect(m_netScreenshotButton, SIGNAL(Clicked()), SLOT(FindNetScreenshot()));

    if (m_trailerButton)
        connect(m_trailerButton, SIGNAL(Clicked()), SLOT(FindTrailer()));

    connect(m_browseCheck, SIGNAL(valueChanged()), SLOT(ToggleBrowse()));
    connect(m_watchedCheck, SIGNAL(valueChanged()), SLOT(ToggleWatched()));

    connect(m_childList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetChild(MythUIButtonListItem*)));
    connect(m_levelList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetLevel(MythUIButtonListItem*)));
    connect(m_categoryList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(SetCategory(MythUIButtonListItem*)));
    connect(m_categoryList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(NewCategoryPopup()));

    return true;
}

namespace
{
    template <typename T>
    struct title_sort
    {
        bool operator()(const T &lhs, const T &rhs)
        {
            return QString::localeAwareCompare(lhs.second, rhs.second) < 0;
        }
    };

    QStringList GetSupportedImageExtensionFilter()
    {
        QStringList ret;

        QList<QByteArray> exts = QImageReader::supportedImageFormats();
        for (QList<QByteArray>::iterator p = exts.begin(); p != exts.end(); ++p)
        {
            ret.append(QString("*.").append(*p));
        }

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

        MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, fp);
        fb->SetNameFilter(GetSupportedImageExtensionFilter());
        if (fb->Create())
        {
            fb->SetReturnEvent(&inst, returnEvent);
            popupStack->AddScreen(fb);
        }
        else
            delete fb;
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
        for (FileAssociations::association_list::const_iterator p =
                fa_list.begin(); p != fa_list.end(); ++p)
        {
            exts << QString("*.%1").arg(p->extension.toUpper());
        }

        MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, fp);
        fb->SetNameFilter(exts);
        if (fb->Create())
        {
            fb->SetReturnEvent(&inst, returnEvent);
            popupStack->AddScreen(fb);
        }
        else
            delete fb;
    }

    const QString CEID_COVERARTFILE = "coverartfile";
    const QString CEID_BANNERFILE = "bannerfile";
    const QString CEID_FANARTFILE = "fanartfile";
    const QString CEID_SCREENSHOTFILE = "screenshotfile";
    const QString CEID_TRAILERFILE = "trailerfile";
    const QString CEID_NEWCATEGORY = "newcategory";
}

void EditMetadataDialog::createBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

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
        m_yearSpin->SetRange(0,9999,1,10);
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
        m_lengthSpin->SetValue(m_workingMetadata->GetLength());
    }

    MythUIButtonListItem *button =
        new MythUIButtonListItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN);
    const VideoCategory::entry_list &vcl =
            VideoCategory::GetCategory().getList();
    for (VideoCategory::entry_list::const_iterator p = vcl.begin();
            p != vcl.end(); ++p)
    {
        button = new MythUIButtonListItem(m_categoryList, p->second);
        button->SetData(p->first);
    }
    m_categoryList->SetValueByData(m_workingMetadata->GetCategoryID());

    for (ParentalLevel i = ParentalLevel::plLowest;
            i <= ParentalLevel::plHigh && i.good(); ++i)
    {
        button = new MythUIButtonListItem(m_levelList,
                                          tr("Level %1").arg(i.GetLevel()));
        button->SetData(i.GetLevel());
    }
    m_levelList->SetValueByData(m_workingMetadata->GetShowLevel());

    //
    //  Fill the "always play this video next" option
    //  with all available videos.
    //

    button = new MythUIButtonListItem(m_childList,tr("None"));

    // TODO: maybe make the title list have the same sort order
    // as elsewhere.
    typedef std::vector<std::pair<unsigned int, QString> > title_list;
    const VideoMetadataListManager::metadata_list &mdl = m_metaCache.getList();
    title_list tc;
    tc.reserve(mdl.size());
    for (VideoMetadataListManager::metadata_list::const_iterator p = mdl.begin();
            p != mdl.end(); ++p)
    {
        QString title;
        if ((*p)->GetSeason() > 0 || (*p)->GetEpisode() > 0)
            title = QString("%1 %2x%3").arg((*p)->GetTitle())
                                       .arg(QString::number((*p)->GetSeason()))
                                       .arg(QString::number((*p)->GetEpisode()));
        else
            title = (*p)->GetTitle();
        tc.push_back(std::make_pair((*p)->GetID(), title));
    }
    std::sort(tc.begin(), tc.end(), title_sort<title_list::value_type>());

    for (title_list::const_iterator p = tc.begin(); p != tc.end(); ++p)
    {
        if (p->first != m_workingMetadata->GetID())
        {
            button = new MythUIButtonListItem(m_childList,p->second);
            button->SetData(p->first);
        }
    }

    if (m_workingMetadata->GetChildID() > 0)
    {
        m_childList->SetValueByData(m_workingMetadata->GetChildID());
        cachedChildSelection = m_workingMetadata->GetChildID();
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
            m_coverart->SetFilename(m_workingMetadata->GetCoverFile());

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
            m_screenshot->SetFilename(m_workingMetadata->GetScreenshot());

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
            m_banner->SetFilename(m_workingMetadata->GetBanner());

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
            m_fanart->SetFilename(m_workingMetadata->GetFanart());

        if (!m_workingMetadata->GetFanart().isEmpty())
            m_fanart->Load();
    }
}

void EditMetadataDialog::NewCategoryPopup()
{
    QString message = tr("Enter new category");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythTextInputDialog *categorydialog =
                                    new MythTextInputDialog(popupStack,message);

    if (categorydialog->Create())
    {
        categorydialog->SetReturnEvent(this, CEID_NEWCATEGORY);
        popupStack->AddScreen(categorydialog);
    }

}

void EditMetadataDialog::AddCategory(QString category)
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
    m_workingMetadata->SetLength(m_lengthSpin->GetIntValue());
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
    cachedChildSelection = item->GetData().toInt();
    m_workingMetadata->SetChildID(cachedChildSelection);
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
        FindImagePopup(url,"",
                       *this, CEID_COVERARTFILE);
    }
    else
        FindImagePopup(gCoreContext->GetSetting("VideoArtworkDir"),
                GetConfDir() + "/MythVideo",
                *this, CEID_COVERARTFILE);
}

void EditMetadataDialog::OnArtworkSearchDone(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    VideoArtworkType type = qVariantValue<VideoArtworkType>(lookup->GetData());
    ArtworkList list = lookup->GetArtwork(type);

    if (list.count() == 0)
        return;

    MythScreenStack *m_popupStack =
                     GetMythMainWindow()->GetStack("popup stack");

    ImageSearchResultsDialog *resultsdialog =
          new ImageSearchResultsDialog(m_popupStack, list, type);

    connect(resultsdialog, SIGNAL(haveResult(ArtworkInfo, VideoArtworkType)),
            SLOT(OnSearchListSelection(ArtworkInfo, VideoArtworkType)));

    if (resultsdialog->Create())
        m_popupStack->AddScreen(resultsdialog);
}

void EditMetadataDialog::OnSearchListSelection(ArtworkInfo info, VideoArtworkType type)
{
    QString msg = tr("Downloading selected artwork...");
    createBusyDialog(msg);

    MetadataLookup *lookup = new MetadataLookup();
    lookup->SetType(kMetadataVideo);
    if (m_workingMetadata->GetSeason() > 0 ||
            m_workingMetadata->GetEpisode() > 0)
        lookup->SetSubtype(kProbableTelevision);
    else if (m_workingMetadata->GetSubtitle().isEmpty())
        lookup->SetSubtype(kProbableMovie);
    else
        lookup->SetSubtype(kUnknownVideo);
    lookup->SetHost(m_workingMetadata->GetHost());
    lookup->SetAutomatic(true);
    lookup->SetData(qVariantFromValue<VideoArtworkType>(type));

    ArtworkMap downloads;
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
        m_busyPopup = NULL;
    }

    VideoArtworkType type = qVariantValue<VideoArtworkType>(lookup->GetData());
    ArtworkMap map = lookup->GetDownloads();

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

    MetadataLookup *lookup = new MetadataLookup();
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
    lookup->SetData(qVariantFromValue<VideoArtworkType>(type));

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
        m_workingMetadata->SetCoverFile(file);

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
        FindImagePopup(url,"",
                *this, CEID_BANNERFILE);
    }
    else
        FindImagePopup(gCoreContext->GetSetting("mythvideo.bannerDir"),
                GetConfDir() + "/MythVideo/Banners",
                *this, CEID_BANNERFILE);
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
        m_workingMetadata->SetBanner(file);

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
        FindImagePopup(url,"",
                *this, CEID_FANARTFILE);
    }
    else
        FindImagePopup(gCoreContext->GetSetting("mythvideo.fanartDir"),
                GetConfDir() + "/MythVideo/Fanart",
                *this, CEID_FANARTFILE);
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
        m_workingMetadata->SetFanart(file);

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
        FindImagePopup(url,"",
                *this, CEID_SCREENSHOTFILE);
    }
    else
        FindImagePopup(gCoreContext->GetSetting("mythvideo.screenshotDir"),
                GetConfDir() + "/MythVideo/Screenshots",
                *this, CEID_SCREENSHOTFILE);
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
        m_workingMetadata->SetScreenshot(file);

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
        FindVideoFilePopup(url,"",
                *this, CEID_TRAILERFILE);
    }
    else
        FindVideoFilePopup(gCoreContext->GetSetting("mythvideo.TrailersDir"),
                GetConfDir() + "/MythVideo/Trailers",
                *this, CEID_TRAILERFILE);
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
        m_workingMetadata->SetTrailer(file);
    CheckedSet(m_trailerText, file);
}

void EditMetadataDialog::customEvent(QEvent *levent)
{
    if (levent->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(levent);

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
        MetadataLookupEvent *lue = (MetadataLookupEvent *)levent;

        MetadataLookupList lul = lue->lookupList;

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
                m_busyPopup = NULL;
            }
        }
    }
    else if (levent->type() == MetadataLookupFailure::kEventType)
    {
        MetadataLookupFailure *luf = (MetadataLookupFailure *)levent;

        MetadataLookupList lul = luf->lookupList;

        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = NULL;
        }

        if (lul.size())
        {
            MetadataLookup *lookup = lul[0];
            LOG(VB_GENERAL, LOG_INFO,
                QString("No results found for %1 %2 %3").arg(lookup->GetTitle())
                    .arg(lookup->GetSeason()).arg(lookup->GetEpisode()));
        }
    }
    else if (levent->type() == ImageDLEvent::kEventType)
    {
        ImageDLEvent *ide = (ImageDLEvent *)levent;

        MetadataLookup *lookup = ide->item;

        if (!lookup)
            return;

        handleDownloadedImages(lookup);
    }
}
