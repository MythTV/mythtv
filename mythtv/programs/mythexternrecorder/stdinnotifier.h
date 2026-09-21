#pragma once

#include <QObject>
#include <QFile>
#include <QSocketNotifier>

#include <QJsonObject>

class StdinNotifier : public QObject
{
    Q_OBJECT

  public:
    explicit StdinNotifier(QObject *parent = nullptr);

  signals:
    void commandReceived(const QJsonObject &jsonMessage);
    void apiVersionRequested();
    void finished();

  private slots:
    void handleReadyRead();

  private:
    QFile m_stdinFile;
    QByteArray m_buffer;
    QSocketNotifier *m_notifier {nullptr};
};
