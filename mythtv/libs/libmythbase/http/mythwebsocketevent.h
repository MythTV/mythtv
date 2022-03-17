#include <QObject>
#include <QString>

#include "libmythbase/http/mythhttpcommon.h"

class MythWebSocketEvent : public QObject
{
    Q_OBJECT

    public:
        MythWebSocketEvent();
        ~MythWebSocketEvent() override;

        void customEvent(QEvent* /*event*/) override; // QObject

        bool HandleTextMessage   (const StringPayload& Text);
        bool HandleRawTextMessage(const DataPayloads& Payloads);
        //void HandleBinaryMessage (const DataPayloads& Payloads);

    signals:
        void SendTextMessage(const QString &);
        void SendBinaryMessage(const QByteArray &);

    private:
        QStringList m_filters;
        bool        m_sendEvents {false}; /// True if the client has enabled events
};
