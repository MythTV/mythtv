#ifndef MYTHDRMPLANE_H
#define MYTHDRMPLANE_H

// MythTV
#include "libmythui/platforms/drm/mythdrmproperty.h"

// libdrm
extern "C" {
#include <drm_fourcc.h>
}

#ifndef DRM_FORMAT_INVALID
#define DRM_FORMAT_INVALID	0
#endif
#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif
#ifndef DRM_FORMAT_NV20
#define DRM_FORMAT_NV20 fourcc_code('N', 'V', '2', '0')
#endif
#ifndef DRM_FORMAT_P210
#define DRM_FORMAT_P210 fourcc_code('P', '2', '1', '0')
#endif
#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010 fourcc_code('P', '0', '1', '0')
#endif
#ifndef DRM_FORMAT_P012
#define DRM_FORMAT_P012 fourcc_code('P', '0', '1', '2')
#endif
#ifndef DRM_FORMAT_P016
#define DRM_FORMAT_P016 fourcc_code('P', '0', '1', '6')
#endif
#ifndef DRM_FORMAT_P030
#define DRM_FORMAT_P030 fourcc_code('P', '0', '3', '0')
#endif
#ifndef DRM_FORMAT_XRGB16161616F
#define DRM_FORMAT_XRGB16161616F fourcc_code('X', 'R', '4', 'H')
#endif
#ifndef DRM_FORMAT_XBGR16161616F
#define DRM_FORMAT_XBGR16161616F fourcc_code('X', 'B', '4', 'H')
#endif
#ifndef DRM_FORMAT_ARGB16161616F
#define DRM_FORMAT_ARGB16161616F fourcc_code('A', 'R', '4', 'H')
#endif
#ifndef DRM_FORMAT_ABGR16161616F
#define DRM_FORMAT_ABGR16161616F fourcc_code('A', 'B', '4', 'H')
#endif

using FOURCCVec = std::vector<uint32_t>;
using DRMPlane  = std::shared_ptr<class MythDRMPlane>;
using DRMPlanes = std::vector<DRMPlane>;

class MUI_PUBLIC MythDRMPlane
{
  public:
    static DRMPlane  Create(int FD, uint32_t Id, uint32_t Index);
    static DRMPlanes GetPlanes(int FD, int CRTCFilter = -1);

    static QString   PlaneTypeToString   (uint64_t Type);
    static DRMPlanes FilterPrimaryPlanes (const DRMPlanes& Planes);
    static DRMPlanes FilterOverlayPlanes (const DRMPlanes& Planes);
    static QString   FormatToString      (uint32_t Format);
    static QString   FormatsToString     (const FOURCCVec& Formats);
    static bool      FormatIsVideo       (uint32_t Format);
    static bool      HasOverlayFormat    (const FOURCCVec& Formats);
    static uint32_t  GetAlphaFormat      (const FOURCCVec& Formats);
    QString          Description() const;

    uint32_t  m_id            { 0 };
    uint32_t  m_index         { 0 };
    uint64_t  m_type          { DRM_PLANE_TYPE_PRIMARY };
    uint32_t  m_possibleCrtcs { 0 };
    uint32_t  m_fbId          { 0 };
    uint32_t  m_crtcId        { 0 };
    DRMProps  m_properties;

    DRMProp   m_fbIdProp      { nullptr };
    DRMProp   m_crtcIdProp    { nullptr };
    DRMProp   m_srcXProp      { nullptr };
    DRMProp   m_srcYProp      { nullptr };
    DRMProp   m_srcWProp      { nullptr };
    DRMProp   m_srcHProp      { nullptr };
    DRMProp   m_crtcXProp     { nullptr };
    DRMProp   m_crtcYProp     { nullptr };
    DRMProp   m_crtcWProp     { nullptr };
    DRMProp   m_crtcHProp     { nullptr };

    FOURCCVec m_formats;
    FOURCCVec m_videoFormats;

  protected:
    MythDRMPlane(int FD, uint32_t Id, uint32_t Index);

  private:
    Q_DISABLE_COPY(MythDRMPlane)
};

#endif
