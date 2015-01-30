#ifndef WEBSOCKET_MYTHEVENT_H_
#define WEBSOCKET_MYTHEVENT_H_

#include "websocket.h"

#include <QStringList>

/** \class WebSocketMythEvent
 *
 *  \brief Extension for sending MythEvents over WebSocketServer
 *
 * \ingroup WebSocket_Extensions
 */
class WebSocketMythEvent : public WebSocketExtension
{
  Q_OBJECT

  public:
    WebSocketMythEvent();
    virtual ~WebSocketMythEvent();

    virtual bool HandleTextFrame(const WebSocketFrame &frame);
    virtual void customEvent(QEvent*);

  private:
    QStringList m_filters;
    bool m_sendEvents; /// True if the client has enabled events
};

#endif
