#include <stdlib.h>

#include <mythtv/mythcontext.h>

#include "globals.h"
#include "videotree.h"
#include "videofilter.h"
#include "videolist.h"
#include "metadata.h"
#include "videoutils.h"
#include "imagecache.h"
#include "parentalcontrols.h"

VideoTree::VideoTree(MythScreenStack *parent, const QString &name,
                         VideoList *video_list, DialogType type)
          : VideoDialog(parent, name, video_list, DLG_TREE)
{
    m_videoButtonTree = NULL;
}

VideoTree::~VideoTree()
{
    if (m_rememberPosition && m_videoButtonTree)
    {
        QStringList path = m_videoButtonTree->GetCurrentNode()->getRouteByString();
        gContext->SaveSetting("mythvideo.VideoTreeLastActive", path.join("\n"));
    }
}

bool VideoTree::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "tree", this);

    if (!foundtheme)
        return false;

    m_videoButtonTree = dynamic_cast<MythUIButtonTree *> (GetChild("videos"));

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));
    m_novideoText = dynamic_cast<MythUIText *> (GetChild("novideos"));
    m_positionText = dynamic_cast<MythUIText *> (GetChild("position"));
    m_crumbText = dynamic_cast<MythUIText *> (GetChild("breadcrumbs"));

    m_coverImage = dynamic_cast<MythUIImage *> (GetChild("coverimage"));

    m_parentalLevelState = dynamic_cast<MythUIStateType *>
                                                    (GetChild("parentallevel"));
    m_videoLevelState = dynamic_cast<MythUIStateType *>
                                                    (GetChild("videolevel"));

    if (m_novideoText)
        m_novideoText->SetText(tr("No Videos Available"));

    if (!m_videoButtonTree)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    if (m_parentalLevelState)
        m_parentalLevelState->DisplayState("None");

    connect(m_videoButtonTree, SIGNAL(itemClicked( MythUIButtonListItem* )),
            this, SLOT( handleSelect( MythUIButtonListItem* )));
    connect(m_videoButtonTree, SIGNAL(itemSelected( MythUIButtonListItem* )),
            this, SLOT( UpdateText( MythUIButtonListItem* )));
    connect(m_videoButtonTree, SIGNAL(nodeChanged( MythGenericTree* )),
            this, SLOT( SetCurrentNode( MythGenericTree* )));
    connect(m_currentParentalLevel, SIGNAL(LevelChanged()),
            this, SLOT(reloadData()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    SetFocusWidget(m_videoButtonTree);
    m_videoButtonTree->SetActive(true);

    int level = gContext->GetNumSetting("VideoDefaultParentalLevel", 1);
    m_currentParentalLevel->SetLevel((ParentalLevel::Level)level);

    return true;
}

void VideoTree::loadData()
{
    m_videoButtonTree->AssignTree(m_rootNode);
}

MythUIButtonListItem* VideoTree::GetItemCurrent()
{
    return m_videoButtonTree->GetItemCurrent();
}
