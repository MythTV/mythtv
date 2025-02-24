#include "mythavbufferref.h"

#include "libmythbase/mythlogging.h"

AVBufferRef* MythAVBufferRef::ref(const AVBufferRef* buf)
{
    if (buf == nullptr)
    {
        return nullptr;
    }
    AVBufferRef* reference = av_buffer_ref(buf);
    if (reference == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, "av_buffer_ref() failed to allocate memory.");
    }
    return reference;
}
