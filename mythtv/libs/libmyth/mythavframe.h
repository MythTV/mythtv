#ifndef MYTHAVFRAME_H
#define MYTHAVFRAME_H

#include "mythexp.h"

extern "C" {
#include "libavcodec/avcodec.h"
}
/** MythAVFrame
 * little utility class that act as a safe way to allocate an AVFrame
 * which can then be allocated on the heap. It simplifies the need to free
 * the AVFrame once done with it.
 * Example of usage:
 * {
 *   MythAVFrame frame;
 *   if (!frame)
 *   {
 *     return false
 *   }
 *
 *   frame->width = 1080;
 *
 *   AVFrame *src = frame;
 * }
 */
class MPUBLIC MythAVFrame
{
  public:
    MythAVFrame(void) : m_frame(av_frame_alloc()) {}
    ~MythAVFrame(void)
    {
        av_frame_free(&m_frame);
    }
    bool operator !() const
    {
        return m_frame == nullptr;
    }
    AVFrame* operator->() const
    {
        return m_frame;
    }
    AVFrame& operator*() const
    {
        return *m_frame;
    }
    operator AVFrame*() const
    {
        return m_frame;
    }
    operator const AVFrame*() const
    {
        return m_frame;
    }

  private:
    AVFrame *m_frame {nullptr};
};

#endif // MYTHAVFRAME_H
