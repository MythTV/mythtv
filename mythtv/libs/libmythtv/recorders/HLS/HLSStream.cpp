#include <unistd.h>

#include <utility>

#include "mythlogging.h"

#include "HLSReader.h"
#include "HLSStream.h"

#define LOC QString("%1 stream: ").arg(m_m3u8Url)

HLSRecStream::HLSRecStream(int seq, uint64_t bitrate, QString  m3u8_url, QString segment_base_url)
    : m_id(seq),
      m_bitrate(bitrate),
      m_m3u8Url(std::move(m3u8_url)),
      m_segmentBaseUrl(std::move(segment_base_url))
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecStream::~HLSRecStream(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "dtor");

#ifdef USING_LIBCRYPTO
    AESKeyMap::iterator Iaes;

    for (Iaes = m_aesKeys.begin(); Iaes != m_aesKeys.end(); ++Iaes)
        delete *Iaes;
#endif
}

QString HLSRecStream::toString(void) const
{
    return QString("%1 bitrate %2").arg(Id()).arg(Bitrate());
}

#ifdef USING_LIBCRYPTO
bool HLSRecStream::DownloadKey(MythSingleDownload& downloader,
                               const QString& keypath, AES_KEY* aeskey)
{
    QByteArray key;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    bool ret = HLSReader::DownloadURL(keypath, &key);
#else
    bool ret = downloader.DownloadURL(keypath, &key);
#endif

    if (!ret || key.size() != AES_BLOCK_SIZE)
    {
        if (ret)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("The AES key loaded doesn't have the right size (%1)")
                .arg(key.size()));
        }
        else
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "Failed to download AES key: " +
                downloader.ErrorString());
        }
        return false;
    }
    AES_set_decrypt_key((const unsigned char*)key.constData(),
                        128, aeskey);
    return true;
}

bool HLSRecStream::DecodeData(MythSingleDownload& downloader,
                              const QByteArray& IV, const QString& keypath,
                              QByteArray& data, int64_t sequence)
{
    AESKeyMap::iterator Ikey;

    if ((Ikey = m_aesKeys.find(keypath)) == m_aesKeys.end())
    {
        auto* key = new AES_KEY;
        DownloadKey(downloader, keypath, key);
        Ikey = m_aesKeys.insert(keypath, key);
        if (Ikey == m_aesKeys.end())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                "DecodeData: Unable to add AES key to map");
            return false;
        }
    }

    /* Decrypt data using AES-128 */
    int aeslen = data.size() & ~0xf;
    std::array<uint8_t,AES_BLOCK_SIZE> iv {};
    char *decrypted_data = new char[data.size()];
    if (IV.isEmpty())
    {
        /*
         * If the EXT-X-KEY tag does not have the IV attribute,
         * implementations MUST use the sequence number of the
         * media file as the IV when encrypting or decrypting that
         * media file.  The big-endian binary representation of
         * the sequence number SHALL be placed in a 16-octet
         * buffer and padded (on the left) with zeros.
         */
        iv[15] = sequence         & 0xff;
        iv[14] = (sequence >> 8)  & 0xff;
        iv[13] = (sequence >> 16) & 0xff;
        iv[12] = (sequence >> 24) & 0xff;
    }
    else
    {
        std::copy(IV.cbegin(), IV.cend(), iv.data());
    }
    AES_cbc_encrypt((unsigned char*)data.constData(),
                    (unsigned char*)decrypted_data, aeslen,
                    *Ikey, iv.data(), AES_DECRYPT);
    std::copy(data.cbegin() + aeslen, data.cend(), decrypted_data + aeslen);

    // remove the PKCS#7 padding from the buffer
    // NOLINTNEXTLINE(bugprone-signed-char-misuse)
    int pad = decrypted_data[data.size()-1];
    if (pad <= 0 || pad > AES_BLOCK_SIZE)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("bad padding character (0x%1)")
            .arg(pad, 0, 16, QLatin1Char('0')));
        delete[] decrypted_data;
        return false;
    }
    aeslen = data.size() - pad;
    data = QByteArray(decrypted_data, aeslen);
    delete[] decrypted_data;

    return true;
}
#endif // USING_LIBCRYPTO

void HLSRecStream::AverageBandwidth(int64_t bandwidth)
{
    // Average the last 20 segments
    if (m_bandwidthSegs.size() > 19)
        m_sumBandwidth -= m_bandwidthSegs.dequeue();
    m_bandwidthSegs.enqueue(bandwidth);
    m_sumBandwidth += bandwidth;
    m_bandwidth = m_sumBandwidth / m_bandwidthSegs.size();
}

std::chrono::seconds HLSRecStream::Duration(void) const
{
    QMutexLocker lock(&m_lock);
    return m_duration;
}

bool HLSRecStream::operator<(const HLSRecStream &b) const
{
    return this->Bitrate() < b.Bitrate();
}

bool HLSRecStream::operator>(const HLSRecStream &b) const
{
    return this->Bitrate() > b.Bitrate();
}

#ifdef USING_LIBCRYPTO
bool HLSRecStream::SetAESIV(QString line)
{
    /*
     * If the EXT-X-KEY tag has the IV attribute, implementations MUST use
     * the attribute value as the IV when encrypting or decrypting with that
     * key.  The value MUST be interpreted as a 128-bit hexadecimal number
     * and MUST be prefixed with 0x or 0X.
     */
    if (!line.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        return false;
    if (line.size() % 2)
    {
        // not even size, pad with front 0
        line.insert(2, QLatin1String("0"));
    }
    int padding = std::max(0, AES_BLOCK_SIZE - (static_cast<int>(line.size()) - 2));
    QByteArray ba = QByteArray(padding, 0x0);
    ba.append(QByteArray::fromHex(QByteArray(line.toLatin1().constData() + 2)));
    m_aesIV = ba;
    m_ivLoaded = true;
    return true;
}
#endif // USING_LIBCRYPTO
