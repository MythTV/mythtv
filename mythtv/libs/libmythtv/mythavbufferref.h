#ifndef LIBMYTHTV_MYTHAVBUFFERREF_H
#define LIBMYTHTV_MYTHAVBUFFERREF_H

#include <utility>

// FFmpeg
extern "C" {
#include "libavutil/buffer.h"
}

/**
@brief C++ wrapper for AVBufferRef.

Using this avoids littering the code with manual memory management.

You must verify that the instance has_buffer() before calling any other function.
*/
class MythAVBufferRef {
  public:
    /**
    @param buf The AVBufferRef* to reference, may be NULL.
    */
    explicit MythAVBufferRef(const AVBufferRef* buf = nullptr) : m_buffer(ref(buf)) {}
    ~MythAVBufferRef() { unref(); }

    // Copy constructor
    MythAVBufferRef(const MythAVBufferRef& other) : MythAVBufferRef(other.m_buffer) {}
    // Move constructor
    MythAVBufferRef(MythAVBufferRef&& other) noexcept : MythAVBufferRef() { swap(*this, other); }
    // Copy assignment operator
    MythAVBufferRef& operator=(MythAVBufferRef other)
    {
        swap(*this, other);
        return *this;
    }
    // Move assignment operator
    MythAVBufferRef& operator=(MythAVBufferRef&& other) noexcept
    {
        // release resources held by this, but prevent suicide on self-assignment
        MythAVBufferRef tmp {std::move(other)};
        swap(*this, tmp);
        return *this;
    }

    friend void swap(MythAVBufferRef& a, MythAVBufferRef& b) noexcept
    {
        using std::swap;
        swap(a.m_buffer, b.m_buffer);
    }

    bool has_buffer()       { return m_buffer != nullptr; }

    const uint8_t*  data()  { return m_buffer->data; }
    size_t          size()  { return m_buffer->size; }
  private:
    static AVBufferRef* ref(const AVBufferRef* buf);
    void unref() { av_buffer_unref(&m_buffer); }

    AVBufferRef* m_buffer {nullptr};
};

#endif // LIBMYTHTV_MYTHAVBUFFERREF_H
