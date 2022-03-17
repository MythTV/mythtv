#ifndef MYTHDVDCONTEXT_H
#define MYTHDVDCONTEXT_H

// MythTV
#include "libmythtv/mythtvexp.h"
#include "libmythbase/referencecounter.h"

// libdvd
#include "dvdnav/dvdnav.h"

/** \class MythDVDContext
 *  \brief Encapsulates playback context at any given moment.
 *
 * This class mainly represents a single VOBU (video object unit) on a DVD
 */
class MTV_PUBLIC MythDVDContext : public ReferenceCounter
{
    friend class MythDVDBuffer;

  public:
    MythDVDContext() = delete;
    ~MythDVDContext() override = default;

    int64_t  GetStartPTS          (void) const;
    int64_t  GetEndPTS            (void) const;
    int64_t  GetSeqEndPTS         (void) const;
    uint32_t GetLBA               (void) const;
    uint32_t GetLBAPrevVideoFrame (void) const;
    int      GetNumFrames         (void) const;
    int      GetNumFramesPresent  (void) const;
    int      GetFPS               (void) const;

  protected:
    MythDVDContext(const dsi_t& DSI, const pci_t& PCI);

  protected:
    dsi_t m_dsi;
    pci_t m_pci;
};

#endif // MYTHDVDCONTEXT_H
