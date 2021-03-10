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
    ~WebSocketMythEvent() override;

    bool HandleTextFrame(const WebSocketFrame &frame) override; // WebSocketExtension
    void customEvent(QEvent* /*event*/) override; // QObject

  protected:
    // No implicit copying.
    WebSocketMythEvent(const WebSocketMythEvent &other) = delete;
    WebSocketMythEvent &operator=(const WebSocketMythEvent &other) = delete;
    WebSocketMythEvent(WebSocketMythEvent &&) = delete;
    WebSocketMythEvent &operator=(WebSocketMythEvent &&) = delete;

  private:
    QStringList m_filters;
    bool        m_sendEvents {false}; /// True if the client has enabled events
};

#endif
