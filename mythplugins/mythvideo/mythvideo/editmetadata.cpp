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
    UIUtilE::Assign(this, m_trailerText, "trailer_text", &err);

    UIUtilE::Assign(this, m_categoryList, "category_select", &err);
    UIUtilE::Assign(this, m_levelList, "level_select", &err);
    UIUtilE::Assign(this, m_childList, "child_select", &err);

    UIUtilE::Assign(this, m_browseCheck, "browse_check", &err);

    UIUtilE::Assign(this, m_coverartButton, "coverart_button", &err);
    UIUtilE::Assign(this, m_doneButton, "done_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'edit_metadata'");
        return false;
    }

    fillWidgets();

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    connect(m_titleEdit, SIGNAL(valueChanged()), SLOT(setTitle()));
    connect(m_playerEdit, SIGNAL(valueChanged()), SLOT(setPlayer()));

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(saveAndExit()));
    connect(m_coverartButton, SIGNAL(Clicked()), SLOT(findCoverArt()));

    connect(m_browseCheck, SIGNAL(valueChanged()), SLOT(toggleBrowse()));

    connect(m_childList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setChild(MythUIButtonListItem*)));
    connect(m_levelList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setLevel(MythUIButtonListItem*)));
    connect(m_categoryList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(setCategory(MythUIButtonListItem*)));
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
}

void EditMetadataDialog::fillWidgets()
{
    m_titleEdit->SetText(m_workingMetadata->Title());

    MythUIButtonListItem *button =
        new MythUIButtonListItem(m_categoryList, VIDEO_CATEGORY_UNKNOWN);
    const VideoCategory::entry_list &vcl =
            VideoCategory::getCategory().getList();
    for (VideoCategory::entry_list::const_iterator p = vcl.begin();
            p != vcl.end(); ++p)
    {
        button = new MythUIButtonListItem(m_categoryList, p->second);
        button->SetData(p->first);
    }
    m_categoryList->SetValueByData(m_workingMetadata->getCategoryID());

    for (ParentalLevel i = ParentalLevel::plLowest;
            i <= ParentalLevel::plHigh && i.good(); ++i)
    {
        button = new MythUIButtonListItem(m_levelList,
                                    QString(tr("Level %1")).arg(i.GetLevel()));
        button->SetData(i.GetLevel());
    }
    m_levelList->SetValueByData(m_workingMetadata->ShowLevel());

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
        tc.push_back(std::make_pair((*p)->ID(), (*p)->Title()));
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
                m_workingMetadata->ChildID() == -1)
            {
                possible_starting_point = p->first;
                m_workingMetadata->setChildID(possible_starting_point);
            }
            trip_catch = false;
        }

        if (p->first != m_workingMetadata->ID())
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

    if (m_workingMetadata->ChildID() > 0)
    {
        m_childList->SetValueByData(m_workingMetadata->ChildID());
        cachedChildSelection = m_workingMetadata->ChildID();
    }
    else
    {
        m_childList->SetValueByData(possible_starting_point);
        cachedChildSelection = possible_starting_point;
    }

    if (m_workingMetadata->Browse())
        m_browseCheck->SetCheckState(MythUIStateType::Full);
    m_coverartText->SetText(m_workingMetadata->CoverFile());
    m_trailerText->SetText(m_workingMetadata->GetTrailer());
    m_playerEdit->SetText(m_workingMetadata->PlayCommand());
}

void EditMetadataDialog::NewCategoryPopup()
{
    QString message = tr("Enter new category");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythTextInputDialog *categorydialog =
                                    new MythTextInputDialog(popupStack,message);

    if (categorydialog->Create())
        popupStack->AddScreen(categorydialog);

    connect(categorydialog, SIGNAL(haveResult(QString)),
            SLOT(AddCategory(QString)), Qt::QueuedConnection);
}

void EditMetadataDialog::AddCategory(QString category)
{
    int id = VideoCategory::getCategory().add(category);
    m_workingMetadata->setCategoryID(id);
    new MythUIButtonListItem(m_categoryList, category, id);
    m_categoryList->SetValueByData(id);
}

void EditMetadataDialog::saveAndExit()
{
    *m_origMetadata = *m_workingMetadata;
    m_origMetadata->updateDatabase();

    emit Finished();
    Close();
}

void EditMetadataDialog::setTitle()
{
    m_workingMetadata->setTitle(m_titleEdit->GetText());
}

void EditMetadataDialog::setCategory(MythUIButtonListItem *item)
{
    m_workingMetadata->setCategoryID(item->GetData().toInt());
}

void EditMetadataDialog::setPlayer()
{
    m_workingMetadata->setPlayCommand(m_playerEdit->GetText());
}

void EditMetadataDialog::setLevel(MythUIButtonListItem *item)
{
    m_workingMetadata->
            setShowLevel(ParentalLevel(item->GetData().toInt()).GetLevel());
}

void EditMetadataDialog::setChild(MythUIButtonListItem *item)
{
    cachedChildSelection = item->GetData().toInt();
    m_workingMetadata->setChildID(cachedChildSelection);
}

void EditMetadataDialog::toggleBrowse()
{
    m_workingMetadata->
            setBrowse(m_browseCheck->GetBooleanCheckState());
}

void EditMetadataDialog::findCoverArt()
{
    QString fileprefix = gContext->GetSetting("VideoArtworkDir");
    // If the video artwork setting hasn't been set default to
    // using ~/.mythtv/MythVideo
    if (fileprefix.isEmpty())
    {
        fileprefix = GetConfDir() + "/MythVideo";
    }

    QStringList imageExtensions;

    QList< QByteArray > exts = QImageReader::supportedImageFormats();
    QList< QByteArray >::Iterator it = exts.begin();
    for (;it != exts.end();++it)
    {
        imageExtensions.append( QString("*.").append(*it) );
    }

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    MythUIFileBrowser *nca = new MythUIFileBrowser(popupStack, fileprefix);
    nca->SetNameFilter(imageExtensions);

    if (nca->Create())
    {
        popupStack->AddScreen(nca);
        connect(nca, SIGNAL(haveResult(QString)), SLOT(SetCoverArt(QString)));
    }
    else
        delete nca;
}

void EditMetadataDialog::SetCoverArt(QString file)
{
    if (file.isEmpty())
        return;

    m_workingMetadata->setCoverFile(file);
    CheckedSet(m_coverartText, file);
}
