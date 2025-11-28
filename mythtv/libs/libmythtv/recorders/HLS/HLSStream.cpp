#include "HLSStream.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>

#if CONFIG_LIBCRYPTO
#include <openssl/aes.h>
#include <openssl/evp.h>
#endif

#include <QMutexLocker>

#include "libmythbase/mythlogging.h"

#ifdef HLS_USE_MYTHDOWNLOADMANAGER
#include "HLSReader.h"
#endif

#define LOC QString("HLSRecstream[%1]: ").arg(m_inputId)

HLSRecStream::HLSRecStream(int inputId, int seq, uint64_t bitrate, QString  m3u8_url, QString segment_base_url)
    : m_inputId(inputId),
      m_id(seq),
      m_bitrate(bitrate),
      m_m3u8Url(std::move(m3u8_url)),
      m_segmentBaseUrl(std::move(segment_base_url))
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecStream::~HLSRecStream(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "dtor");
#if CONFIG_LIBCRYPTO
    AESKeyMap::iterator Iaes;

    for (Iaes = m_aesKeys.begin(); Iaes != m_aesKeys.end(); ++Iaes)
        delete *Iaes;
#endif  // CONFIG_LIBCRYPTO
}

QString HLSRecStream::toString(void) const
{
    return QString("%1 bitrate %2").arg(Id()).arg(Bitrate());
}

#if CONFIG_LIBCRYPTO
bool HLSRecStream::DownloadKey(MythSingleDownload& downloader,
                               const QString& keypath, HLS_AES_KEY* aeskey) const
{
    QByteArray key;

#ifdef HLS_USE_MYTHDOWNLOADMANAGER // MythDownloadManager leaks memory
    bool ret = HLSReader::DownloadURL(keypath, &key);
#else
    bool ret = downloader.DownloadURL(keypath, &key);
#endif

    if (!ret || key.size() != AES128_KEY_SIZE)
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
    memcpy(aeskey->key.data(), key.constData(), AES128_KEY_SIZE);

    LOG(VB_RECORD, LOG_DEBUG, LOC + "Downloaded AES key");

    return true;
}

// AES decryption based on OpenSSL example found on
// https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
//
int HLSRecStream::Decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
                          unsigned char *iv, unsigned char *plaintext) const
{
    int len = 0;

    int plaintext_len = 0;

    /* Create and initialise the context */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to create and initialize cipher context");
        return 0;
    }

    /*
     * Initialise the decryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 128 bit AES (i.e. a 128 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to initialize decryption operation");
        return 0;
    }

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary.
     */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to decrypt");
        return 0;
    }
    plaintext_len = len;

    /*
     * Finalise the decryption. Further plaintext bytes may be written at
     * this stage.
     */
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to finalize decryption" +
            QString(" len:%1").arg(len) +
            QString(" plaintext_len:%1").arg(plaintext_len) );
        return 0;
    }
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}

bool HLSRecStream::DecodeData(MythSingleDownload& downloader,
                              const QByteArray& IV, const QString& keypath,
                              QByteArray& data, int64_t sequence)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "DecodeData:" +
        QString(" IV.size():%1").arg(IV.size()) +
        QString(" keypath:%1..%2").arg(keypath.left(20),keypath.right(20)) +
        QString(" sequence:%1").arg(sequence));

    AESKeyMap::iterator Ikey = m_aesKeys.find(keypath);
    if (Ikey == m_aesKeys.end())
    {
        auto* key = new HLS_AES_KEY;
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
    std::array<uint8_t,AES_BLOCK_SIZE> iv {};
    auto *decrypted_data = new uint8_t[data.size()];
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

    int aeslen = data.size() & ~0xf;
    if (aeslen != data.size())
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Data size %1 not multiple of 16 bytes, rounding to %2")
                .arg(data.size()).arg(aeslen));
    }

    int plaintext_len = Decrypt((unsigned char*)data.constData(), aeslen, (*Ikey)->key.data(),
                                 iv.data(), decrypted_data);

    LOG(VB_RECORD, LOG_INFO, LOC +
        QString("Segment data.size()):%1 plaintext_len:%2")
            .arg(data.size()).arg(plaintext_len));

    data = QByteArray(reinterpret_cast<char*>(decrypted_data), plaintext_len);
    delete[] decrypted_data;

    return true;
}
#endif  // CONFIG_LIBCRYPTO

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
