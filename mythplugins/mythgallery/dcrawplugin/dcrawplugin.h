#include <QByteArray>
#include <QIODevice>
#include <QImageIOHandler>
#include <QImageIOPlugin>
#include <QObject>
#include <QStringList>

class DcrawPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface"
                      FILE "raw.json");

public:

    QStringList keys() const;
    Capabilities capabilities(QIODevice *device,
                              const QByteArray &format) const override; // QImageIOPlugin
    QImageIOHandler *create(QIODevice *device,
                            const QByteArray &format) const override; // QImageIOPlugin
};
