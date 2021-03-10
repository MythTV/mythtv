// MythTV
#include "audioplayer.h"
#include "osd.h"
#include "mythplayerstate.h"

/*! \class MythOverlayState
 * \brief A simpler than simple wrapper around OSD state.
*/
MythOverlayState::MythOverlayState(bool Browsing, bool Editing)
  : m_browsing(Browsing),
    m_editing(Editing)
{
}

/*! \class MythAudioState
 * \brief A simple wrapper around audio state used to signal changes
 * in the current state.
*/
MythAudioState::MythAudioState(AudioPlayer* Player, std::chrono::milliseconds Offset)
  : m_hasAudioOut(Player->HasAudioOut()),
    m_volumeControl(Player->ControlsVolume()),
    m_volume(Player->GetVolume()),
    m_muteState(Player->GetMuteState()),
    m_canUpmix(Player->CanUpmix()),
    m_isUpmixing(Player->IsUpmixing()),
    m_paused(Player->IsPaused()),
    m_audioOffset(Offset)
{
}

MythNavigationState::MythNavigationState(int CurrentChapter, std::vector<int64_t> ChapterTimes,
                                         int CurrentTitle,   std::vector<int64_t> TitleDurations, std::vector<QString> TitleNames,
                                         int CurrentAngle,   std::vector<QString> AngleNames)
  : m_currentChapter(CurrentChapter),
    m_chapterTimes(std::move(ChapterTimes)),
    m_currentTitle(CurrentTitle),
    m_titleDurations(std::move(TitleDurations)),
    m_titleNames(std::move(TitleNames)),
    m_currentAngle(CurrentAngle),
    m_angleNames(std::move(AngleNames))
{
}

MythVideoBoundsState::MythVideoBoundsState(AdjustFillMode AdjustFill, AspectOverrideMode AspectOverride,
                                           float HorizScale, float VertScale, QPoint Move,
                                           StereoscopicMode StereoOverride)
  : m_adjustFillMode(AdjustFill),
    m_aspectOverrideMode(AspectOverride),
    m_manualHorizScale(HorizScale),
    m_manualVertScale(VertScale),
    m_manualMove(Move),
    m_stereoOverride(StereoOverride)
{
}

MythVideoColourState::MythVideoColourState(PictureAttributeSupported Supported,
                                           std::map<PictureAttribute, int> AttributeValues)
  : m_supportedAttributes(Supported),
    m_attributeValues(std::move(AttributeValues))
{
}

int MythVideoColourState::GetValue(PictureAttribute Attribute)
{
    if (auto attr = m_attributeValues.find(Attribute); attr != m_attributeValues.end())
        return attr->second;
    return -1;
}

MythVisualiserState::MythVisualiserState(bool Embedding, bool Visualising,
                                         QString Name, QStringList Visualisers)
  : m_embedding(Embedding),
    m_visualising(Visualising),
    m_visualiserName(std::move(Name)),
    m_visualiserList(std::move(Visualisers))
{
}

MythEditorState::MythEditorState(uint64_t Current, uint64_t Previous, uint64_t Next, uint64_t Total,
                                 bool InDelete, bool IsTemp, bool HasTemp,
                                 bool HasUndo, QString Undo, bool HasRedo, QString Redo,
                                 bool Saved)
  : m_currentFrame(Current),
    m_previousCut(Previous),
    m_nextCut(Next),
    m_totalFrames(Total),
    m_frameInDelete(InDelete),
    m_isTempMark(IsTemp),
    m_hasTempMark(HasTemp),
    m_hasUndo(HasUndo),
    m_undoMessage(std::move(Undo)),
    m_hasRedo(HasRedo),
    m_redoMessage(std::move(Redo)),
    m_saved(Saved)
{
}
