#include <QObject>
#include <QString>

#include "libmythbase/http/mythhttpcommon.h"

class MythWebSocketEvent : public QObject
{
    Q_OBJECT

    public:
        MythWebSocketEvent();
        ~MythWebSocketEvent() override;

        bool HandleTextMessage   (const StringPayload& Text);
        bool HandleRawTextMessage(const DataPayloads& Payloads);
        //void HandleBinaryMessage (const DataPayloads& Payloads);

    signals:
        void SendTextMessage(const QString &);
        void SendBinaryMessage(const QByteArray &);

    protected:
        void customEvent(QEvent* /*event*/) override; // QObject

    private:
        QStringList m_filters;
        bool        m_sendEvents {false}; /// True if the client has enabled events
};
