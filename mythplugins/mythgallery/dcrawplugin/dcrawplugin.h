#include <QByteArray>
#include <QIODevice>
#include <QImageIOHandler>
#include <QImageIOPlugin>
#include <QObject>
#include <QStringList>

class DcrawPlugin : public QImageIOPlugin
{

    Q_OBJECT

public:

    QStringList keys() const;
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format) const;

};

