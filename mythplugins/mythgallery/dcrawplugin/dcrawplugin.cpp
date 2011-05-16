#include "dcrawplugin.h"

#include "dcrawformats.h"
#include "dcrawhandler.h"

#include <QByteArray>
#include <QImageIOHandler>
#include <QImageIOPlugin>
#include <QIODevice>
#include <QStringList>

QStringList DcrawPlugin::keys() const
{
    return QStringList(DcrawFormats::getFormats().toList());
}

QImageIOPlugin::Capabilities DcrawPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (DcrawFormats::getFormats().contains(format))
        return CanRead;

    if (format.isEmpty())
    {
        DcrawHandler handler;
        handler.setDevice(device);
        if (handler.canRead())
            return CanRead;
    }

    return 0;
}

QImageIOHandler *DcrawPlugin::create(QIODevice *device, const QByteArray &format) const
{
    DcrawHandler *handler = new DcrawHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

// This shouldn't be necessary, but it shuts up the dang compiler warning.
QObject* qt_plugin_instance_dcrawplugin();

Q_EXPORT_PLUGIN2(dcrawplugin, DcrawPlugin)

