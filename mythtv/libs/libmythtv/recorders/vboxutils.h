#ifndef VBOX_UTILS_H
#define VBOX_UTILS_H

// C++ headers
#include <utility>

// Qt headers
#include <QDomDocument>
#include <QString>

// MythTV headers
#include "channelscan/vboxchannelfetcher.h"

static constexpr const char* VBOX_MIN_API_VERSION  { "VB.2.50" };

class VBox
{
  public:
    explicit VBox(QString url) :
        m_url(std::move(url)) {}

    ~VBox(void) = default;

    static QStringList probeDevices(void);
    static QString getIPFromVideoDevice(const QString &dev);

    bool isConnected(void);
    bool checkConnection(void);
    bool checkVersion(QString &version);
    QDomDocument *getBoardInfo(void);
    QStringList getTuners(void);
    vbox_chan_map_t *getChannels(void);

  protected:
    enum ErrorCode : std::uint8_t
    {
        SUCCESS = 0,
        UNKNOWN_METHOD = 1,
        GENERAL_ERROR = 2,
        MISSING_PARAMETER = 3,
        ILLEGAL_PARAMETER = 4,
        REQUEST_REJECTED = 5,
        MISSING_METHOD = 6,
        REQUEST_TIMEOUT = 7,
        REQUEST_ABOTRED = 8
    };

    static bool sendQuery(const QString &query, QDomDocument *xmlDoc);

    QString m_url;

  private:
    static QStringList doUPNPSearch(void);
    static QString getFirstText(QDomElement &element);
    static QString getStrValue(const QDomElement &element, const QString &name, int index = 0);
    static int getIntValue(const QDomElement &element, const QString &name, int index = 0);
};
#endif // VBOX_UTILS_H
