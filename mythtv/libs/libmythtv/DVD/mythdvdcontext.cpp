// Qt
#include <QString>

// MythTV
#include "mythdvdcontext.h"

MythDVDContext::MythDVDContext(const dsi_t& DSI, const pci_t& PCI)
  : ReferenceCounter("MythDVDContext"),
    m_dsi(DSI),
    m_pci(PCI)
{
}

int64_t MythDVDContext::GetStartPTS(void) const
{
    return static_cast<int64_t>(m_pci.pci_gi.vobu_s_ptm);
}

int64_t MythDVDContext::GetEndPTS(void) const
{
    return static_cast<int64_t>(m_pci.pci_gi.vobu_e_ptm);
}

int64_t MythDVDContext::GetSeqEndPTS(void) const
{
    return static_cast<int64_t>(m_pci.pci_gi.vobu_se_e_ptm);
}

uint32_t MythDVDContext::GetLBA(void) const
{
    return m_pci.pci_gi.nv_pck_lbn;
}

/** \brief Returns the duration of this VOBU in frames
 *  \sa GetNumFramesPresent
 */
int MythDVDContext::GetNumFrames() const
{
    return static_cast<int>(((GetEndPTS() - GetStartPTS()) * GetFPS()) / 90000);
}

/** \brief Returns the number of video frames present in this VOBU
 *  \sa GetNumFrames
 */
int MythDVDContext::GetNumFramesPresent() const
{
    int frames = 0;

    if (GetSeqEndPTS())
    {
        // Sequence end PTS is set.  This means that video frames
        // are not present all the way to 'End PTS'
        frames = static_cast<int>(((GetSeqEndPTS() - GetStartPTS()) * GetFPS()) / 90000);
    }
    else if (m_dsi.dsi_gi.vobu_1stref_ea != 0)
    {
        // At least one video frame is present
        frames = GetNumFrames();
    }

    return frames;
}

int MythDVDContext::GetFPS(void) const
{
    return (m_pci.pci_gi.e_eltm.frame_u & 0x80) ? 30 : 25;
}

/** \brief Returns the logical block address of the previous
 * VOBU containing video.
 * \return LBA or 0xbfffffff if no previous VOBU with video exists
 */
uint32_t MythDVDContext::GetLBAPrevVideoFrame() const
{
    uint32_t lba = m_dsi.vobu_sri.prev_video;

    if (lba != 0xbfffffff)
    {
        // If there is a previous video frame in this
        // cell, calculate the absolute LBA from the
        // offset
        lba = GetLBA() - (lba & 0x7ffffff);
    }

    return lba;
}
