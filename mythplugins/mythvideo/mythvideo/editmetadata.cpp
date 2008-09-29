// C++ headers
#include <iostream>
#include <algorithm>

// Myth headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>
#include <mythtv/mythverbose.h>

// MythUI headers
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythdialogbox.h>

// MythVideo headers
#include "globals.h"
#include "editmetadata.h"
#include "metadata.h"
#include "dbaccess.h"
#include "metadatalistmanager.h"
#include "videoutils.h"
#include "parentalcontrols.h"
#include "videopopups.h"


EditMetadataDialog::EditMetadataDialog(MythScreenStack *parent,
                                       const QString &name,
                                       Metadata *source_metadata,
                                       const MetadataListManager &cache)
    : MythScreenType(parent, name), m_origMetadata(source_metadata),
    m_metaCache(cache)
{
    //
    //  The only thing this screen does is let the
    //  user set (some) metadata information. It only
    //  works on a single metadata entry.
    //

    //
    //  Make a copy, so we can abandon changes if desired
    //

    m_workingMetadata = new Metadata(*m_origMetadata);

    m_categoryList = m_levelList = m_childList = NULL;
    m_browseCheck = NULL;
    m_coverartButton = m_doneButton = NULL;
    m_coverartText = NULL;
    m_titleEdit = m_playerEdit = NULL;
}

EditMetadataDialog::~EditMetadataDialog()
{
    if (m_workingMetadata)
    {
        delete m_workingMetadata;
        m_workingMetadata = NULL;
    }
}

bool EditMetadataDialog::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "edit_metadata", this);

    if (!foundtheme)
        return false;

    m_titleEdit = dynamic_cast<MythUITextEdit *> (GetChild("title_edit"));
    m_playerEdit = dynamic_cast<MythUITextEdit *> (GetChild("player_edit"));

    m_coverartText = dynamic_cast<MythUIText *> (GetChild("coverart_text"));

    m_categoryList = dynamic_cast<MythUIButtonList *>(GetChild("category_select"));
    m_levelList = dynamic_cast<MythUIButtonList *>(GetChild("level_select"));
    m_childList = dynamic_cast<MythUIButtonList *>(GetChild("child_select"));

    m_browseCheck = dynamic_cast<MythUICheckBox *>(GetChild("browse_check"));

    m_coverartButton = dynamic_cast<MythUIButton *>
                                         (GetChild("coverart_button"));
    m_doneButton = dynamic_cast<MythUIButton *> (GetChild("done_button"));

    if (!m_titleEdit || !m_playerEdit || !m_categoryList || !m_levelList
        || !m_childList || !m_browseCheck
        || !m_coverartButton || !m_doneButton || !m_coverartText)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    m_doneButton->SetText(tr("Done"));

    fillWidgets();

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    connect(m_titleEdit, SIGNAL(valueChanged()), SLOT(setTitle()));
    connect(m_playerEdit, SIGNAL(valueChanged()), SLOT(setPlayer()));

    connect(m_doneButton, SIGNAL(buttonPressed()), SLOT(saveAndExit()));
    connect(m_coverartButton, SIGNAL(buttonPressed()), SLOT(findCoverArt()));

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
    //
    //  Persist to database
    //

    *m_origMetadata = *m_workingMetadata;
    m_origMetadata->updateDatabase();

    //
    //  All done
    //
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
    ParentalLevel nl(item->GetData().toInt());
    m_workingMetadata->setShowLevel(nl.GetLevel());
}

void EditMetadataDialog::setChild(MythUIButtonListItem *item)
{
    int new_child = item->GetData().toInt();
    m_workingMetadata->setChildID(new_child);
    cachedChildSelection = new_child;
}

void EditMetadataDialog::toggleBrowse()
{
    MythUIStateType::StateType state = m_browseCheck->GetCheckState();
    if (state == MythUIStateType::Off)
        m_workingMetadata->setBrowse(false);
    else if (state == MythUIStateType::Full)
        m_workingMetadata->setBrowse(true);
}

void EditMetadataDialog::findCoverArt()
{
    QString new_coverart_file;
    if (!isDefaultCoverFile(m_workingMetadata->CoverFile()))
    {
        new_coverart_file = m_workingMetadata->CoverFile();
    }

    QString fileprefix = gContext->GetSetting("VideoArtworkDir");
    // If the video artwork setting hasn't been set default to
    // using ~/.mythtv/MythVideo
    if (fileprefix.length() == 0)
    {
        fileprefix = GetConfDir() + "/MythVideo";
    }

//     QStringList imageExtensions;
//
//     QList< QByteArray > exts = QImageReader::supportedImageFormats();
//
//     for (QList< QByteArray >::Iterator it  = exts.begin();
//                                        it != exts.end();
//                                      ++it )
//     {
//         imageExtensions.append( QString( *it ) );
//     }

//     MythImageFileDialog *nca =
//         new MythImageFileDialog(&new_coverart_file,
//                                 fileprefix,
//                                 gContext->GetMainWindow(),
//                                 "file_chooser",
//                                 "video-",
//                                 "image file chooser",
//                                 true);
//     nca->exec();
//     if (new_coverart_file.length() > 0)
//     {
//         m_workingMetadata->setCoverFile(new_coverart_file);
//         checkedSetText(m_coverartText, new_coverart_file);
//     }
//
//     nca->deleteLater();
}
