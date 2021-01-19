// MythTV
#include "platforms/drm/mythdrmencoder.h"

/*! \class MythDRMEncoder
 * \brief A simple object representing a DRM encoder.
 *
 * The full list of available encoders can be retrieved with GetEncoders and a
 * specific encoder retrieved from a list with GetEncoder.
*/
DRMEnc MythDRMEncoder::Create(int FD, uint32_t Id)
{
    if (FD && Id)
        if (auto e = std::shared_ptr<MythDRMEncoder>(new MythDRMEncoder(FD, Id)); e.get() && e->m_id)
            return e;

    return nullptr;
}

DRMEnc MythDRMEncoder::GetEncoder(const DRMEncs& Encoders, uint32_t Id)
{
    auto match = [&Id](const auto & Enc) { return Enc->m_id == Id; };
    if (auto found = std::find_if(Encoders.cbegin(), Encoders.cend(), match); found != Encoders.cend())
        return *found;
    return nullptr;
}

DRMEncs MythDRMEncoder::GetEncoders(int FD)
{
    DRMEncs result;
    if (auto resources = MythDRMResources(FD); *resources)
    {
        for (auto i = 0; i < resources->count_encoders; ++i)
            if (auto encoder = Create(FD, resources->encoders[i]); encoder.get())
                result.emplace_back(encoder);
    }
    return result;
}

MythDRMEncoder::MythDRMEncoder(int FD, uint32_t Id)
{
    if (auto * encoder = drmModeGetEncoder(FD, Id); encoder)
    {
        m_id = encoder->encoder_id;
        m_type = encoder->encoder_type;
        m_crtcId = encoder->crtc_id;
        m_possibleCrtcs = encoder->possible_crtcs;
        drmModeFreeEncoder(encoder);
    }
}

