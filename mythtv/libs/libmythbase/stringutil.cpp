#include "stringutil.h"

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif

bool StringUtil::isValidUTF8(const QByteArray& data)
{
    // NOTE: If you have a better way to determine this, then please update this
    // Any chosen method MUST be able to identify UTF-8 encoded text without
    // using the BOM Byte-Order Mark as that will probably not be present.
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QTextCodec::ConverterState state;
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    const QString text = codec->toUnicode(data.constData(), data.size(), &state);
    if (state.invalidChars > 0)
        return false;
#else
    auto toUtf16 = QStringDecoder(QStringDecoder::Utf8);
    QString text = toUtf16.decode(data);
    if (toUtf16.hasError())
        return false;
#endif

    Q_UNUSED(text);
    return true;
}

/**
 * \brief Guess whether a string is UTF-8
 *
 * \note  This does not attempt to \e validate the whole string.
 *        It just checks if it has any UTF-8 sequences in it.
 *
 *  \todo FIXME? skips first byte, not sure the second to last if statement makes sense
 */

bool StringUtil::hasUtf8(const char *str)
{
    const unsigned char *c = (unsigned char *) str;

    while (*c++)
    {
        // ASCII is < 0x80.
        // 0xC2..0xF4 is probably UTF-8.
        // Anything else probably ISO-8859-1 (Latin-1, Unicode)

        if (*c > 0xC1 && *c < 0xF5)
        {
            int bytesToCheck = 2;  // Assume  0xC2-0xDF (2 byte sequence)

            if (*c > 0xDF)         // Maybe   0xE0-0xEF (3 byte sequence)
                ++bytesToCheck;
            if (*c > 0xEF)         // Matches 0xF0-0xF4 (4 byte sequence)
                ++bytesToCheck;

            while (bytesToCheck--)
            {
                ++c;

                if (! *c)                    // String ended in middle
                    return false;            // Not valid UTF-8

                if (*c < 0x80 || *c > 0xBF)  // Bad UTF-8 sequence
                    break;                   // Keep checking in outer loop
            }

            if (!bytesToCheck)  // Have checked all the bytes in the sequence
                return true;    // Hooray! We found valid UTF-8!
        }
    }

    return false;
}
