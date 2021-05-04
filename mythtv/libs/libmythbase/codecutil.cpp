
#include "codecutil.h"

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif

bool CodecUtil::isValidUTF8(const QByteArray& data)
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
