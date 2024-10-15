// Wrapper for AVFormatContext (av_open_input_file/stream)
// supporting myth remote files
#ifndef REMOTEAVFORMATCONTEXT_H
#define REMOTEAVFORMATCONTEXT_H

#include <QString>

#include <libmythbase/mythlogging.h>
#include <libmythbase/remotefile.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

class ArchiveRemoteAVFormatContext
{
  public:
    explicit ArchiveRemoteAVFormatContext(const QString &filename = "")
    { if (!filename.isEmpty()) Open(filename); }

    ~ArchiveRemoteAVFormatContext()
    {
        Close();
        if (m_buffer)
            av_free(m_buffer);
    }

    bool Open(const QString &filename)
    {
        if (isOpen())
            return false;

        if (m_inputFC)
            avformat_free_context(m_inputFC);
        m_inputFC = avformat_alloc_context();

        delete m_rf;

        m_inputIsRemote = filename.startsWith("myth://");
        if (m_inputIsRemote)
        {
            m_rf = new RemoteFile(filename);

            if (!m_rf->isOpen())
                return false;

            const int BUFFER_SIZE = 0x8000;
            if (!m_buffer)
            {
                m_buffer = (unsigned char*)av_malloc(BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
                if (!m_buffer)
                    return false;
            }
            m_byteIOContext = avio_alloc_context(m_buffer, BUFFER_SIZE, 0,
                    m_rf, &ReadFunc, &WriteFunc, &SeekFunc);

            m_byteIOContext->seekable = 1;

            // probe the stream
            AVProbeData probe_data;
            memset(&probe_data, 0, sizeof(AVProbeData));
            probe_data.filename = "stream";
            probe_data.buf_size = m_rf->Read(m_buffer, BUFFER_SIZE);
            probe_data.buf = m_buffer;

            const AVInputFormat *fmt = av_probe_input_format(&probe_data, 1);
            if (!fmt)
                return false;

            m_rf->Seek(0, SEEK_SET);

            m_inputFC->pb = m_byteIOContext;

            int ret = avformat_open_input(&m_inputFC, "stream", fmt, nullptr);
            if (ret)
                return false;
        }
        else
        {
            int ret = avformat_open_input(&m_inputFC,  qPrintable(filename), nullptr, nullptr);
            if (ret)
                return false;
        }

        return true;
    }

    void Close()
    {
        if (m_inputFC)
        {
            avformat_close_input(&m_inputFC);
            m_inputFC = nullptr;
        }

        if (m_rf)
        {
            delete m_rf;
            m_rf = nullptr;
        }
    }

    bool isOpen() const
    {
        if (m_inputIsRemote)
            return (m_inputFC != nullptr && m_rf != nullptr && m_rf->isOpen());
        return (m_inputFC != nullptr);
    }

    operator AVFormatContext * () const { return m_inputFC; }
    AVFormatContext * operator -> () const { return m_inputFC; }

  private:
    static int ReadFunc(void *opaque, uint8_t *buf, int buf_size)
    {
        auto *rf = reinterpret_cast< RemoteFile* >(opaque);
        return rf->Read(buf, buf_size);
    }

    static int WriteFunc(void */*opaque*/, const uint8_t */*buf*/, int /*buf_size*/)
        {  return -1; }

    static int64_t SeekFunc(void *opaque, int64_t offset, int whence)
    {
        auto *rf = reinterpret_cast< RemoteFile* >(opaque);
        if (whence == AVSEEK_SIZE)
            return rf->GetFileSize();

        return rf->Seek(offset, whence);
    }

  private:
    AVFormatContext *m_inputFC       { nullptr };
    bool             m_inputIsRemote {   false };
    RemoteFile      *m_rf            { nullptr };
    AVIOContext     *m_byteIOContext { nullptr };
    unsigned char   *m_buffer        { nullptr };
};
#endif // REMOTEAVFORMATCONTEXT_H
