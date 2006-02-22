#include "dvbdescriptors.h"

#include <qtextcodec.h>

// Decode a text string according to ETSI EN 300 468 Annex A
QString dvb_decode_text(const unsigned char *src, uint length)
{
    QString result;

    if (!length)
        return QString("");

    unsigned char buf[length];
    memcpy(buf, src, length);

    if ((buf[0] <= 0x10) || (buf[0] >= 0x20))
    {
        // Strip formatting characters
        for (uint p = 0; p < length; p++)
        {
            if ((buf[p] >= 0x80) && (buf[p] <= 0x9F))
                memmove(buf + p, buf + p + 1, --length - p);
        }

        if (buf[0] >= 0x20)
        {
            result = QString::fromLatin1((const char*)buf, length);
        }
        else if ((buf[0] >= 0x01) && (buf[0] <= 0x0B))
        {
            QString coding = "ISO8859-" + QString::number(4 + buf[0]);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*)buf + 1, length - 1);
        }
        else if (buf[0] == 0x10)
        {
            // If the first byte of the text field has a value "0x10"
            // then the following two bytes carry a 16-bit value (uimsbf) N
            // to indicate that the remaining data of the text field is
            // coded using the character code table specified by
            // ISO Standard 8859, parts 1 to 9

            uint code = 1;
            swab(buf + 1, &code, 2);
            QString coding = "ISO8859-" + QString::number(code);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*)buf + 3, length - 3);
        }
        else
        {
            // Unknown/invalid encoding - assume local8Bit
            result = QString::fromLocal8Bit((const char*)buf + 1, length - 1);
        }
    }
    else
    {
        // TODO: Handle multi-byte encodings

        VERBOSE(VB_SIPARSER, "dvbdescriptors.cpp: "
                "Multi-byte coded text - not supported!");
        result = "N/A";
    }

    return result;
}
