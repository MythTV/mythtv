#ifndef MYTHEDID_H
#define MYTHEDID_H

// Qt
#include <QStringList>

class MythEDID
{
  public:
    MythEDID(QByteArray &Data);
    MythEDID(const char* Data, int Length);

    bool        Valid             (void);
    QStringList SerialNumbers     (void);
    int         PhysicalAddress   (void);
    int         AudioLatency      (bool Interlaced);
    int         VideoLatency      (bool Interlaced);

  private:
    void        Parse(void);

    bool        m_valid           { false };
    QByteArray  m_data            { };
    QStringList m_serialNumbers   { };
    int         m_physicalAddress { 0 };
    int         m_audioLatency[2] { 0 };
    int         m_videoLatency[2] { 0 };
};

#endif // MYTHEDID_H
