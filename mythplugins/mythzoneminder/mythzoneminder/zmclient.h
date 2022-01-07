#ifndef ZMCLIENT_H_
#define ZMCLIENT_H_

#include <array>
#include <iostream>
#include <vector>

// myth
#include <mythsocket.h>
#include <mythexp.h>
#include <mythimage.h>

// zm
#include "zmdefines.h"
#define MAX_IMAGE_SIZE  (2048*1536*3)
using FrameData = std::array<uint8_t,MAX_IMAGE_SIZE>;

class MPUBLIC ZMClient : public QObject
{
    Q_OBJECT

  protected:
    ZMClient();

    static ZMClient *m_zmclient;

  public:
    ~ZMClient() override;

    static ZMClient *get(void);
    static bool setupZMClient (void);

    void customEvent(QEvent *event) override; // QObject

    // Used to actually connect to an ZM server
    bool connectToHost(const QString &hostname, unsigned int port);
    bool connected(void) const { return m_bConnected; }

    bool checkProtoVersion(void);

    // If you want to be pleasant, call shutdown() before deleting your ZMClient 
    // device
    void shutdown();

    void getServerStatus(QString &status, QString &cpuStat, QString &diskStat);
    void updateMonitorStatus(void);
    void getEventList(const QString &monitorName, bool oldestFirst,
                      const QString &date, bool includeContinuous, std::vector<Event*> *eventList);
    void getEventFrame(Event *event, int frameNo, MythImage **image);
    void getAnalyseFrame(Event *event, int frameNo, QImage &image);
    int  getLiveFrame(int monitorID, QString &status, FrameData& buffer);
    void getFrameList(int eventID, std::vector<Frame*> *frameList);
    void deleteEvent(int eventID);
    void deleteEventList(std::vector<Event*> *eventList);

    int  getMonitorCount(void);
    Monitor* getMonitorAt(int pos);
    Monitor* getMonitorByID(int monID);

    void getCameraList(QStringList &cameraList);
    void getEventDates(const QString &monitorName, bool oldestFirst, 
                       QStringList &dateList);
    void setMonitorFunction(int monitorID, const QString &function, bool enabled);
    bool updateAlarmStates(void);

    bool isMiniPlayerEnabled(void) const { return m_isMiniPlayerEnabled; }
    void setIsMiniPlayerEnabled(bool enabled) { m_isMiniPlayerEnabled = enabled; }

    void saveNotificationMonitors(void);
    void showMiniPlayer(int monitorID) const;

  private slots:
    void restartConnection(void);  // Try to re-establish the connection to 
                                   // ZMServer every 10 seconds
  private:
    void doGetMonitorList(void);
    bool readData(unsigned char *data, int dataSize);
    bool sendReceiveStringList(QStringList &strList);

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex              m_listLock          {QMutex::Recursive};
    QMutex              m_commandLock       {QMutex::Recursive};
#else
    QRecursiveMutex     m_listLock;
    QRecursiveMutex     m_commandLock;
#endif
    QList<Monitor*>     m_monitorList;
    QMap<int, Monitor*> m_monitorMap;

    MythSocket       *m_socket              {nullptr};
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex            m_socketLock          {QMutex::Recursive};
#else
    QRecursiveMutex   m_socketLock;
#endif
    QString           m_hostname            {"localhost"};
    uint              m_port                {6548};
    bool              m_bConnected          {false};
    QTimer           *m_retryTimer          {nullptr};
    bool              m_zmclientReady       {false};
    bool              m_isMiniPlayerEnabled {true};
};

#endif
