#ifndef MEDIASERVERUTIL_H_
#define MEDIASERVERUTIL_H_

using namespace std;

#include <QString>
#include <QDateTime>

#include "mythprotoserverexp.h"
#include "programinfo.h"
#include "referencecounter.h"

class DeleteThread;

class PROTOSERVER_PUBLIC DeleteHandler : public ReferenceCounter
{
    Q_OBJECT
  public:
    DeleteHandler(void);
    DeleteHandler(QString filename);
   ~DeleteHandler(void);

    void Close(void);

    QString     GetPath(void)           { return m_path;    }
    int         GetFD(void)             { return m_fd;      }
    off_t       GetSize(void)           { return m_size;    }
    QDateTime   GetWait(void)           { return m_wait;    }

    void        SetPath(QString path)   { m_path= path;     }

    virtual void DeleteSucceeded(void)  {};
    virtual void DeleteFailed(void)     {};

    friend class DeleteThread;

  private:
    QString     m_path;
    int         m_fd;
    off_t       m_size;
    QDateTime   m_wait;

};

QString GetPlaybackURL(ProgramInfo *pginfo, bool storePath = true);

#endif
