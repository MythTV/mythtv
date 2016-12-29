// Wrapper for AVFormatContext (av_open_input_file/stream)
// supporting myth remote files
#ifndef REMOTEAVFORMATCONTEXT_H
#define REMOTEAVFORMATCONTEXT_H

#include <iostream>
#include <QString>

#include <remotefile.h>
#include <mythlogging.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include "libavutil/opt.h"
}

class RemoteAVFormatContext
{
  public:
    explicit RemoteAVFormatContext(const QString &filename = "") :
        m_inputFC(NULL), m_inputIsRemote(false), m_isOpen(false),
        m_rf(NULL), m_byteIOContext(NULL), m_buffer(NULL)
    { if (!filename.isEmpty()) Open(filename); }

    ~RemoteAVFormatContext()
    {
        Close();
        if (m_buffer)
            av_free(m_buffer);
    }

    AVFormatContext *getContext(void) { return m_inputFC; }

    bool Open(const QString &filename)
    {
        LOG(VB_PLAYBACK, LOG_ERR,  QString("RemoteAVFormatContext::Open: Opening %1").arg(filename));
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

            const int BUFFER_SIZE = 0x20000;
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
            memset(&probe_data, 0, sizeof(AVProbeData));
            probe_data.filename = "stream";
            probe_data.buf_size = m_rf->Read(m_buffer, BUFFER_SIZE);
            probe_data.buf = m_buffer;

            AVInputFormat *fmt = av_probe_input_format(&probe_data, 1);
            if (!fmt)
            {
                LOG(VB_GENERAL, LOG_ERR,  QString("RemoteAVFormatContext::Open: Failed to probe file: %1").arg(filename));
                return false;
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_INFO,  QString("RemoteAVFormatContext::Open: probed file as %1").arg(fmt->name));
            }

            m_rf->Seek(0, SEEK_SET);

            m_inputFC->pb = m_byteIOContext;

            int ret = avformat_open_input(&m_inputFC, "stream", fmt, NULL);
            if (ret)
            {
                LOG(VB_GENERAL, LOG_ERR,  QString("RemoteAVFormatContext::Open: Failed to open input: %1").arg(filename));
                return false;
            }
        }
        else
        {
            // if this is a ice/shoutcast stream setup grabbing the inline metadata
            AVDictionary *options = NULL;

            if (filename.startsWith("http://"))
                av_dict_set(&options, "icy", "1", 0);

            int ret = avformat_open_input(&m_inputFC,  qPrintable(filename), NULL, &options);
            if (ret)
            {
                LOG(VB_GENERAL, LOG_ERR,  QString("RemoteAVFormatContext::Open: Failed to open input: %1").arg(filename));
                return false;
            }
        }

        m_isOpen = true;
        return m_isOpen;
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

        m_isOpen = false;
    }

    bool isOpen() const { return m_isOpen; }

    operator AVFormatContext * () const { return m_inputFC; }
    //operator AVFormatContext & () const { return m_inputFC; }
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
    bool m_isOpen;
    RemoteFile *m_rf;
    AVIOContext *m_byteIOContext;
    unsigned char *m_buffer;
};
#endif // REMOTEAVFORMATCONTEXT_H
