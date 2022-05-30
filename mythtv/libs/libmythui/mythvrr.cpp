// libmythbase
#include "libmythbase/mythlogging.h"

// libmythui
#include "mythvrr.h"
#ifdef USING_DRM
#include "platforms/mythdisplaydrm.h"
#include "platforms/drm/mythdrmvrr.h"
#endif
#ifdef USING_X11
#include "platforms/mythdisplayx11.h"
#include "platforms/mythnvcontrol.h"
#endif
#include "mythdisplay.h"

// Qt
#include <QObject>

#define LOC QString("VRR: ")

/*! \class MythVRR
 *
 * MythVRR is the base class for Variable Refresh Rate support. Concrete
 * implementations are currently provided by MythGSync (for NVidia GSync) and
 * MythDRMVRR (for FreeSync/Adaptive-Sync)
 *
 * \note Variable refresh rate works well with the MythTV rendering code though
 * there may be some occasional 'unusual' behaviour. For example, when video is
 * behind audio during playback, instead of dropping frames, the player will typically
 * display a number of frames in rapid succession to catch up and as a result it
 * looks as if the video is fast forwarded for a fraction of a second.
 *
 * \note Neither GSync or FreeSync can be enabled or disabled once the application
 * window has been created. To that end, the focus of support is based on detecting
 * the current state and adapting behaviours when VRR is enabled (e.g. we do not
 * switch video modes purely for a change in refresh rate). To try and change
 * the VRR mode when the MythTV application starts, use the -vrr/--vrr command
 * line option; which tries to either enable or disable VRR before the window is
 * created and resets VRR to the default state when the application quits. (There
 * is no setting for this option as it must be used before we have initialised Qt
 * and hence there is no database connection).
 *
 * \sa MythGSync
 * \sa MythDRMVRR
*/
MythVRR::MythVRR(bool Controllable, VRRType Type, bool Enabled, MythVRRRange Range)
  : m_controllable(Controllable),
    m_type(Type),
    m_enabled(Enabled),
    m_range(std::move(Range))
{
}

/*! \brief Create a concrete implementation of MythVRR suitable for the given Display
*/
MythVRRPtr MythVRR::Create(MythDisplay* MDisplay)
{
    if (!MDisplay)
        return nullptr;

    MythVRRPtr result = nullptr;

#if defined (USING_X11) || defined (USING_DRM)
    const auto range = MDisplay->GetEDID().GetVRRRange();

#ifdef USING_X11
    // GSync is only available with X11 over Display Port
    if (auto nvcontrol = MythNVControl::Create(); nvcontrol)
        if (auto gsync = MythGSync::CreateGSync(nvcontrol, range); gsync)
            result = gsync;
#endif

#ifdef USING_DRM
    // FreeSync is only currently *controllable* via DRM with an AMD GPU/APU and Display Port
    if (!result)
    {
        if (auto * display = dynamic_cast<MythDisplayDRM*>(MDisplay); display && display->GetDevice())
            if (auto freesync = MythDRMVRR::CreateFreeSync(display->GetDevice(), range); freesync)
                result =  freesync;
    }

    // If we don't have support for controlling FreeSync then DRM may still be able to
    // tell us if it is available/enabled - which is still useful
    if (!result)
    {
        if (auto drm = MythDRMDevice::Create(MDisplay->GetCurrentScreen(), DRM_QUIET); drm)
            if (auto freesync = MythDRMVRR::CreateFreeSync(drm, range); freesync)
                result = freesync;
    }
#endif
#endif

    if (!result)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No variable refresh rate support detected");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("VRR type '%1': Enabled: %2 Controllable: %3 %4")
            .arg(result->TypeToString()).arg(result->Enabled()).arg(result->IsControllable())
            .arg(result->RangeDescription()));
    }
    return result;
}

bool MythVRR::Enabled() const
{
    return m_enabled;
}

MythVRRRange MythVRR::GetRange() const
{
    return m_range;
}

bool MythVRR::IsControllable() const
{
    return m_controllable;
}

QString MythVRR::TypeToString() const
{
    switch (m_type)
    {
        case FreeSync:    return QObject::tr("FreeSync");
        case GSync:       return QObject::tr("GSync");
        case GSyncCompat: return QObject::tr("GSync Compatible");
        default: break;
    }
    return QObject::tr("None");
}

QString MythVRR::RangeDescription() const
{
    if (std::get<0>(m_range) > 0 && std::get<1>(m_range) > 0)
    {
        return QObject::tr("Range: %1-%2%3")
            .arg(std::get<0>(m_range)).arg(std::get<1>(m_range))
            .arg(std::get<2>(m_range) ? "" : " (Estimated)");
    }
    return {};
}

