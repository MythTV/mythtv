
#include "codecutil.h"

#include <QTextCodec>

bool CodecUtil::isValidUTF8(const QByteArray& data)
{
    // NOTE: If you have a better way to determine this, then please update this
    // Any chosen method MUST be able to identify UTF-8 encoded text without
    // using the BOM Byte-Order Mark as that will probably not be present.
    QTextCodec::ConverterState state;
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    const QString text = codec->toUnicode(data.constData(), data.size(), &state);
    if (state.invalidChars > 0)
        return false;

    Q_UNUSED(text);
    return true;
}
