// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef INPUT_H
#define INPUT_H

class StreamInput;

#include <qurl.h>
#include <qsocket.h>


class StreamInput : public QObject
{
    Q_OBJECT
public:
    StreamInput(const QUrl &);

    QIODevice *socket() { return sock; }

    void setup();


private slots:
    void hostfound();
    void connected();
    void readyread();
    void error(int);


private:
    QCString request;
    QUrl url;
    QSocket *sock;
    int stage;
};


#endif // INPUT_H
