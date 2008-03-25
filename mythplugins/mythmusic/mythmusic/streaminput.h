//Added by qt3to4:
#include <Q3CString>
// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef INPUT_H
#define INPUT_H

class StreamInput;

#include <q3url.h>
#include <q3socket.h>


class StreamInput : public QObject
{
    Q_OBJECT
public:
    StreamInput(const Q3Url &);

    QIODevice *socket() { return sock; }

    void setup();


private slots:
    void hostfound();
    void connected();
    void readyread();
    void error(int);


private:
    Q3CString request;
    Q3Url url;
    Q3Socket *sock;
    int stage;
};


#endif // INPUT_H
