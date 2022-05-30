// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdisplay.h"
#include "libmythui/mythedid.h"

#include "mythavutil.h"
#include "mythframe.h"
#include "mythhdrtracker.h"

#define LOC QString("HDRTracker: ")

/*! \brief Create a tracker instance that looks for changes in the required EOTF
 *
 * An instance is only created if the display supports at least one HDR format *and*
 * the instance is capable of signalling the HDR metadata to the display (which
 * is currently limited to DRM on linux - with no X11 or Wayland).
*/
HDRTracker MythHDRTracker::Create(MythDisplay* MDisplay)
{
    if (!MDisplay)
        return nullptr;

    // Check for HDR support
    auto hdr = MDisplay->GetHDRState();
    if (!hdr.get())
        return nullptr;

    // We only support HDR10 and HLG
    if (!hdr->m_supportedTypes)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "No HDR support detected for this display");
        return nullptr;
    }

    // We need to be able to set metadata
    if (!hdr->IsControllable())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "HDR signalling support not available");
        return nullptr;
    }

    return std::shared_ptr<MythHDRTracker>(new MythHDRTracker(hdr));
}

MythHDRTracker::MythHDRTracker(MythHDRPtr HDR)
  : m_hdr(std::move(HDR))
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Tracking HDR signalling for: %1")
        .arg(m_hdr->TypesToString().join(",")));
}

MythHDRTracker::~MythHDRTracker()
{
    Reset();
}

void MythHDRTracker::Reset()
{
    // TODO Revert to the pre-existing state - don't assume SDR
    if (m_hdr && m_hdr->m_currentType != MythHDR::SDR)
        m_hdr->SetHDRMetadata(MythHDR::SDR, nullptr);
    m_metadata = nullptr;
}

/*! \brief Track changes to the HDR type (EOTF) and request a change when needed.
 *
 * \note Signalling is only requested when a change is seen; this is all static
 * metadata currently (i.e. no HDR10+ support - which is frame by frame). But there
 * may be an advantage to signalling every frame or at least every X frames as I'm
 * not sure this is 100% reliable.
*/
void MythHDRTracker::Update(MythVideoFrame* Frame)
{
    if (!Frame || !m_hdr)
        return;

    // Do we have an HDR EOTF that is supported by the display?
    if (auto eotf = MythAVUtil::FFmpegTransferToHDRType(Frame->m_colortransfer);
        eotf & m_hdr->m_supportedTypes)
    {
        if (eotf == MythHDR::HLG)
        {
            if (m_hdr->m_currentType != MythHDR::HLG)
            {
                m_hdr->SetHDRMetadata(MythHDR::HLG, nullptr);
                m_metadata = nullptr;
            }
        }
        else
        {
            // TODO Better change tracking when there is no metadata (i.e. Frame->m_hdrMetadata is null)
            if (m_hdr->m_currentType != MythHDR::HDR10 || !m_metadata ||
                (Frame->m_hdrMetadata.get() && !m_metadata->Equals(Frame->m_hdrMetadata.get())))
            {
                m_metadata = Frame->m_hdrMetadata ?
                    std::make_shared<MythHDRVideoMetadata>(*(Frame->m_hdrMetadata.get())) :
                    std::make_shared<MythHDRVideoMetadata>();
                m_hdr->SetHDRMetadata(MythHDR::HDR10, m_metadata);
            }
        }
    }
    else
    {
        Reset();
    }
}

