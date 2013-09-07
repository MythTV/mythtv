// Wrapper for AVFormatContext (av_open_input_file/stream)
// supporting myth remote files
#ifndef REMOTEAVFORMATCONTEXT_H
#define REMOTEAVFORMATCONTEXT_H

#include <QString>

#include <remotefile.h>
#include <mythlogging.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

class RemoteAVFormatContext
{
  public:
    RemoteAVFormatContext(const QString &filename = "") :
        m_inputFC(NULL), m_inputIsRemote(false), m_rf(NULL), m_byteIOContext(NULL), m_buffer(NULL)
    { if (!filename.isEmpty()) Open(filename); }

    ~RemoteAVFormatContext()
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

        if (m_rf)
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
                m_buffer = (unsigned char*)av_malloc(BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
                if (!m_buffer)
                    return false;
            }
            m_byteIOContext = avio_alloc_context(m_buffer, BUFFER_SIZE, 0,
                    m_rf, &ReadFunc, &WriteFunc, &SeekFunc);

            m_byteIOContext->seekable = 1;

            // probe the stream
            AVProbeData probe_data;
            probe_data.filename = "stream";
            probe_data.buf_size = m_rf->Read(m_buffer, BUFFER_SIZE);
            probe_data.buf = m_buffer;

            AVInputFormat *fmt = av_probe_input_format(&probe_data, 1);
            if (!fmt)
                return false;

            m_rf->Seek(0, SEEK_SET);

            m_inputFC->pb = m_byteIOContext;

            int ret = avformat_open_input(&m_inputFC, "stream", fmt, NULL);
            if (ret)
                return false;
        }
        else
        {
            const char *type = NULL;
            AVInputFormat *fmt = type ? av_find_input_format(type) : NULL;

            int ret = avformat_open_input(&m_inputFC,  qPrintable(filename), fmt, NULL);
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
            m_inputFC = NULL;
        }

        if (m_rf)
        {
            delete m_rf;
            m_rf = NULL;
        }
    }

    bool isOpen() const
    {
        if (m_inputIsRemote)
            return (m_inputFC != NULL && m_rf != NULL && m_rf->isOpen());
        else
            return (m_inputFC != NULL);
    }

    operator AVFormatContext * () const { return m_inputFC; }
    AVFormatContext * operator -> () const { return m_inputFC; }

  private:
    static int ReadFunc(void *opaque, uint8_t *buf, int buf_size)
    {
        RemoteFile *rf = reinterpret_cast< RemoteFile* >(opaque);
        return rf->Read(buf, buf_size);
    }

    static int WriteFunc(void *, uint8_t *, int) {  return -1; }

    static int64_t SeekFunc(void *opaque, int64_t offset, int whence)
    {
        RemoteFile *rf = reinterpret_cast< RemoteFile* >(opaque);
        if (whence == AVSEEK_SIZE)
            return rf->GetFileSize();

        return rf->Seek(offset, whence);
    }

  private:
    AVFormatContext *m_inputFC;
    bool m_inputIsRemote;
    RemoteFile *m_rf;
    AVIOContext *m_byteIOContext;
    unsigned char *m_buffer;
};
#endif // REMOTEAVFORMATCONTEXT_H
