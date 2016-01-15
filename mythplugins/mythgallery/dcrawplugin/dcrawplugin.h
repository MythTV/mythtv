#include <QByteArray>
#include <QIODevice>
#include <QImageIOHandler>
#include <QImageIOPlugin>
#include <QObject>
#include <QStringList>

class DcrawPlugin : public QImageIOPlugin
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface"
                      FILE "raw.json")
#endif

public:

    QStringList keys() const;
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format) const;
};

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // This shouldn't be necessary, but it shuts up the dang compiler warning.
    const QStaticPlugin qt_static_plugin_DcrawPlugin();
#endif