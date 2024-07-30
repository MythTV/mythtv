/* MHEG Interaction Channel
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#ifndef MHEGIC_H
#define MHEGIC_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QMutex>
#include <QHash>

class QByteArray;
class NetStream;

class MHInteractionChannel : public QObject
{
    Q_OBJECT

public:
    explicit MHInteractionChannel(QObject* parent = nullptr);
    ~MHInteractionChannel() override;

    // Properties
public:
    // Get network status
    enum EStatus : std::uint8_t { kActive = 0, kInactive, kDisabled };
    static EStatus status();

    // Operations
public:
    // Is a file ready to read?
    bool CheckFile(const QString &csPath, const QByteArray &cert = QByteArray());
    // Read a file
    enum EResult : std::int8_t { kError = -1, kSuccess = 0, kPending = 1 };
    EResult GetFile(const QString &csPath, QByteArray &data,
                    const QByteArray &cert = QByteArray() );

    // Implementation
private slots:
    // NetStream signals
    void slotFinished(QObject *obj);

private:
    Q_DISABLE_COPY(MHInteractionChannel)
    mutable QMutex m_mutex;
    using map_t = QHash< QUrl, NetStream* >;
    map_t m_pending; // Pending requests
    map_t m_finished; // Completed requests
};

#endif /* ndef MHEGIC_H */
