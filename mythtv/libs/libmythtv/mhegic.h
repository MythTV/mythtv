/* MHEG Interaction Channel
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#ifndef MHEGIC_H
#define MHEGIC_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QHash>

class QByteArray;
class NetStream;

class MHInteractionChannel : public QObject
{
    Q_OBJECT

public:
    MHInteractionChannel(QObject* parent = 0);
    virtual ~MHInteractionChannel();

    // Properties
public:
    // Get network status
    enum EStatus { kActive = 0, kInactive, kDisabled };
    static EStatus status();

    // Operations
public:
    // Is a file ready to read?
    bool CheckFile(const QString &url, const QByteArray &cert = QByteArray());
    // Read a file
    enum EResult { kError = -1, kSuccess = 0, kPending };
    EResult GetFile(const QString &url, QByteArray &data,
                    const QByteArray &cert = QByteArray() );

    // Implementation
private slots:
    // NetStream signals
    void slotFinished(QObject*);

private:
    Q_DISABLE_COPY(MHInteractionChannel)
    mutable QMutex m_mutex;
    typedef QHash< QString, NetStream* > map_t;
    map_t m_pending; // Pending requests
    map_t m_finished; // Completed requests
};

#endif /* ndef MHEGIC_H */
