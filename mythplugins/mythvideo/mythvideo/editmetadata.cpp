#include <algorithm>

#include <QImageReader>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuicheckbox.h>

#include "globals.h"
#include "dbaccess.h"
#include "metadatalistmanager.h"
#include "videoutils.h"
#include "editmetadata.h"

EditMetadataDialog::EditMetadataDialog(MythScreenStack *lparent,
        QString lname, Metadata *source_metadata,
        const MetadataListManager &cache) : MythScreenType(lparent, lname),
    m_origMetadata(source_metadata), m_titleEdit(0), m_playerEdit(0),
    m_categoryList(0), m_levelList(0), m_childList(0), m_browseCheck(0),
    m_coverartButton(0), m_coverartText(0),
    m_screenshotButton(0), m_screenshotText(0),
    m_bannerButton(0), m_bannerText(0),
    m_fanartButton(0), m_fanartText(0),
    m_trailerButton(0), m_trailerText(0),
    m_doneButton(0), cachedChildSelection(0),
    m_metaCache(cache)
{
    m_workingMetadata = new Metadata(*m_origMetadata);
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
    UIUtilE::Assign(this, m_playerEdit, "player_edit", &err);

    UIUtilE::Assign(this, m_coverartText, "coverart_text", &err);
    UIUtilE::Assign(this, m_screenshotText, "screenshot_text", &err);
    UIUtilE::Assign(this, m_bannerText, "banner_text", &err);
    UIUtilE::Assign(this, m_fanartText, "fanart_text", &err);
    UIUtilE::Assign(this, m_trailerText, "trailer_text", &err);

    UIUtilE::Assign(this, m_categoryList, "category_select", &err);
    UIUtilE::Assign(this, m_levelList, "level_select", &err);
    UIUtilE::Assign(this, m_childList, "child_select", &err);

    UIUtilE::Assign(this, m_browseCheck, "browse_check", &err);

    UIUtilE::Assign(this, m_coverartButton, "coverart_button", &err);
    UIUtilE::Assign(this, m_bannerButton, "banner_button", &err);
    UIUtilE::Assign(this, m_fanartButton, "fanart_button", &err);
    UIUtilE::Assign(this, m_screenshotButton, "screenshot_button", &err);
    UIUtilE::Assign(this, m_doneButton, "done_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'edit_metadata'");
        return false;
    }

    fillWidgets();

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    connect(m_titleEdit, SIGNAL(valueChanged()), SLOT(SetTitle()));
    connect(m_playerEdit, SIGNAL(valueChanged()), SLOT(SetPlayer()));

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(SaveAndExit()));
    connect(m_coverartButton, SIGNAL(Clicked()), SLOT(FindCoverArt()));
    connect(m_bannerButton, SIGNAL(Clicked()), SLOT(FindBanner()));
    connect(m_fanartButton, SIGNAL(Clicked()), SLOT(FindFanart()));
    connect(m_screenshotButton, SIGNAL(Clicked()), SLOT(FindScreenshot()));

    connect(m_browseCheck, SIGNAL(valueChanged()), SLOT(ToggleBrowse()));

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
        QString fp = prefix.isEmpty() ? prefixAlt : prefix;

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

    const QString CEID_COVERARTFILE = "coverartfile";
    const QString CEID_BANNERFILE = "bannerfile";
    const QString CEID_FANARTFILE = "fanartfile";
    const QString CEID_SCREENSHOTFILE = "screenshotfile";
    const QString CEID_NEWCATEGORY = "newcategory";
}

void EditMetadataDialog::fillWidgets()
{
    m_titleEdit->SetText(m_workingMetadata->GetTitle());

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
                                    QString(tr("Level %1")).arg(i.GetLevel()));
        button->SetData(i.GetLevel());
    }
    m_levelList->SetValueByData(m_workingMetadata->GetShowLevel());

    //
    //  Fill the "always play this video next" option
    //  with all available videos.
    //

    bool trip_catch = false;
    QString caught_name = "";
    int possible_starting_point = 0;

    button = new MythUIButtonListItem(m_childList,tr("None"));

    // TODO: maybe make the title list have the same sort order
    // as elsewhere.
    typedef std::vector<std::pair<unsigned int, QString> > title_list;
    const MetadataListManager::metadata_list &mdl = m_metaCache.getList();
    title_list tc;
    tc.reserve(mdl.size());
    for (MetadataListManager::metadata_list::const_iterator p = mdl.begin();
            p != mdl.end(); ++p)
    {
        tc.push_back(std::make_pair((*p)->GetID(), (*p)->GetTitle()));
    }
    std::sort(tc.begin(), tc.end(), title_sort<title_list::value_type>());

    for (title_list::const_iterator p = tc.begin(); p != tc.end(); ++p)
    {
        if (trip_catch)
        {
            //
            //  Previous loop told us to check if the two
            //  movie names are close enough to try and
            //  set a default starting point.
            //

            QString target_name = p->second;
            int length_compare = 0;
            if (target_name.length() < caught_name.length())
            {
                length_compare = target_name.length();
            }
            else
            {
                length_compare = caught_name.length();
            }

            QString caught_name_three_quarters =
                    caught_name.left((int)(length_compare * 0.75));
            QString target_name_three_quarters =
                    target_name.left((int)(length_compare * 0.75));

            if (caught_name_three_quarters == target_name_three_quarters &&
                m_workingMetadata->GetChildID() == -1)
            {
                possible_starting_point = p->first;
                m_workingMetadata->SetChildID(possible_starting_point);
            }
            trip_catch = false;
        }

        if (p->first != m_workingMetadata->GetID())
        {
            button = new MythUIButtonListItem(m_childList,p->second);
            button->SetData(p->first);
        }
        else
        {
            //
            //  This is the current file. Set a flag so the default
            //  selected child will be set next loop
            //

            trip_catch = true;
            caught_name = p->second;
        }
    }

    if (m_workingMetadata->GetChildID() > 0)
    {
        m_childList->SetValueByData(m_workingMetadata->GetChildID());
        cachedChildSelection = m_workingMetadata->GetChildID();
    }
    else
    {
        m_childList->SetValueByData(possible_starting_point);
        cachedChildSelection = possible_starting_point;
    }

    if (m_workingMetadata->GetBrowse())
        m_browseCheck->SetCheckState(MythUIStateType::Full);
    m_coverartText->SetText(m_workingMetadata->GetCoverFile());
    m_screenshotText->SetText(m_workingMetadata->GetScreenshot());
    m_bannerText->SetText(m_workingMetadata->GetBanner());
    m_fanartText->SetText(m_workingMetadata->GetFanart());
    m_trailerText->SetText(m_workingMetadata->GetTrailer());
    m_playerEdit->SetText(m_workingMetadata->GetPlayCommand());
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

void EditMetadataDialog::SetCategory(MythUIButtonListItem *item)
{
    m_workingMetadata->SetCategoryID(item->GetData().toInt());
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

void EditMetadataDialog::FindCoverArt()
{
    FindImagePopup(gContext->GetSetting("VideoArtworkDir"),
            GetConfDir() + "/MythVideo",
            *this, CEID_COVERARTFILE);
}

void EditMetadataDialog::SetCoverArt(QString file)
{
    if (file.isEmpty())
        return;

    m_workingMetadata->SetCoverFile(file);
    CheckedSet(m_coverartText, file);
}

void EditMetadataDialog::FindBanner()
{
    FindImagePopup(gContext->GetSetting("mythvideo.bannerDir"),
            GetConfDir() + "/MythVideo/Banners",
            *this, CEID_BANNERFILE);
}

void EditMetadataDialog::SetBanner(QString file)
{
    if (file.isEmpty())
        return;

    m_workingMetadata->SetBanner(file);
    CheckedSet(m_bannerText, file);
}

void EditMetadataDialog::FindFanart()
{
    FindImagePopup(gContext->GetSetting("mythvideo.fanartDir"),
            GetConfDir() + "/MythVideo/Fanart",
            *this, CEID_FANARTFILE);
}

void EditMetadataDialog::SetFanart(QString file)
{
    if (file.isEmpty())
        return;

    m_workingMetadata->SetFanart(file);
    CheckedSet(m_fanartText, file);
}

void EditMetadataDialog::FindScreenshot()
{
    FindImagePopup(gContext->GetSetting("mythvideo.screenshotDir"),
            GetConfDir() + "/MythVideo/Screenshots",
            *this, CEID_SCREENSHOTFILE);
}

void EditMetadataDialog::SetScreenshot(QString file)
{
    if (file.isEmpty())
        return;

    m_workingMetadata->SetScreenshot(file);
    CheckedSet(m_screenshotText, file);
}

void EditMetadataDialog::customEvent(QEvent *levent)
{
    if (levent->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                dynamic_cast<DialogCompletionEvent*>(levent);

        if (!dce)
            return;

        const QString resultid = dce->GetId();

        if (resultid == CEID_COVERARTFILE)
            SetCoverArt(dce->GetResultText());
        else if (resultid == CEID_BANNERFILE)
            SetBanner(dce->GetResultText());
        else if (resultid == CEID_FANARTFILE)
            SetFanart(dce->GetResultText());
        else if (resultid == CEID_SCREENSHOTFILE)
            SetScreenshot(dce->GetResultText());
        else if (resultid == CEID_NEWCATEGORY)
            AddCategory(dce->GetResultText());
    }
}
