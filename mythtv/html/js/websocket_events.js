
// Initialise a global gWSHandler on load - there should only be one handler
// per browser instance. One socket with multiple 'listeners'
var globalWSHandler = {};
function InitWebSocketEventHandler()
{
    console.log("Websocket Init");
    globalWSHandler = new WebSocketEventHandler("ws://" + document.domain + ":" + (Number(location.port) + 5) + "/");
}
window.addEventListener("DOMContentLoaded", InitWebSocketEventHandler);

/*!
 * \Class WebSocketEventClient
 */
var WebSocketEventClient = function() {
    this.name = "Unnamed";
    this.eventReceiver = {};
    this.statusReceiver = {};
    this.filters = new Array();
}

/*!
 * \Class WebSocketEventHandler
 *
 * The WebSocketEventHandler handles MythEvents sent over a WebSocket connection
 *
 * It allows multiple listeners to be created with only one socket open. Each
 * listener registers the events that it would like to receive. The combined
 * event filters are sent to the backend which will only send events that at
 * least one listener is expecting, keeping network traffic to a minimum.
 *
 * A listener creates a _WebSocketEventClient_, populating the _eventReceiver_
 * with the function which will handle the event and _filters_ with an array of
 * event names. The _WebSocketEventClient_ object is then provided as the
 * argument to _WebSocketEventHandler.AddListener_
 *
 * To stop listening, call WebSocketEventHandler.RemoveListener
 *
 */
var WebSocketEventHandler = function (uri) {
    this.uri = uri;
    this.websocket = null;
    this.listeners = new Array();
    this.filters = new Array();
    this.writeQueue = new Array();
    this.reconnectTimerId = null;
    this.isReconnecting = false;
    this.Open();
};

/*!
 * \fn void OnOpen
 * \private
 *
 * Handler for WebSocket open events
 */
WebSocketEventHandler.prototype.OnOpen = function(wsEvent) {

    // Enable sending of events from server
    this.writeQueue.push("WS_EVENT_ENABLE");

    if (this.isReconnecting)
    {
        console.log("WebSocketEventHandler: Socket Reconnected");
        this.isReconnecting = false;
        // Cancel the reconnect timer
        window.clearInterval(this.reconnectTimerId);
        this.reconnectTimerId = null;
        // Resend the filters to the server (calls ProcessWriteQueue)
        this.UpdateFilters();
    }
    else
    {
        console.log("WebSocketEventHandler: Socket Open");
        // Process any writes which were made while we were still connecting
        this.ProcessWriteQueue();
    }
}

/*!
 * \fn void OnClose
 * \private
 *
 * Handler for WebSocket close events
 */
WebSocketEventHandler.prototype.OnClose = function(wsEvent) {

    // Reset the websocket var to allow for reconnection
    this.websocket = null;
    if (wsEvent.code > WebSocket.CLOSE_GOING_AWAY) // Only log non-standard close events
        console.log("WebSocketEventHandler: Close " + wsEvent.code + " " + wsEvent.reason);
    else if (!this.isReconnecting)
    {
        console.log("WebSocketEventHandler: Connection Lost");
        // Note: Don't try to reconnect if the server closed the connection
        // with a code > 1001, it had a good reason and we might end up in
        // a loop.

        // Set a reconnecting flag, prevents us starting multiple timers among
        // other things
        this.isReconnecting = true;
        // Enable reconnect timer - fires every 10 seconds
        var _self = this;
        this.reconnectTimerId = window.setInterval(function () { _self.Reconnect(); }, 10000);
    }
}

/*!
 * \fn void OnMessage
 * \private
 *
 * Handler for WebSocket message events
 */
WebSocketEventHandler.prototype.OnMessage = function(wsEvent) {
    var eventStr = wsEvent.data;
    var eventName = eventStr.split(" ")[0];
    for (i = 0; i < this.listeners.length; i++) {
        var listener = this.listeners[i];
        // Does listener want to receive this event?
        if (listener.filters.indexOf(eventName) != -1)
            listener.eventReceiver(eventStr);
    }
}

/*!
 * \fn void OnMessage
 * \private
 *
 * Handler for WebSocket message events
 */
WebSocketEventHandler.prototype.OnError = function(wsEvent) {
    if (!this.isReconnecting)
        console.log("WebSocketEventHandler: Error Occurred");
}

/*!
 * \fn void Open
 * \public
 *
 * Open the websocket connection
 */
WebSocketEventHandler.prototype.Open = function() {
    if (this.websocket && typeof this.websocket !== undefined &&
        this.websocket !== null)
    {
        if (this.websocket.readyState == WebSocket.OPEN ||
            this.websocket.readyState == WebSocket.CONNECTING)
        {
            console.log("WebSocketEventHandler: Open called on already open socket");
            return;
        }
    }

    var _self = this;
    this.websocket = new WebSocket(this.uri);
    this.websocket.onopen = function(wsEvent) { _self.OnOpen(wsEvent) };
    this.websocket.onclose = function(wsEvent) { _self.OnClose(wsEvent) };
    this.websocket.onmessage = function(wsEvent) { _self.OnMessage(wsEvent) };
    this.websocket.onerror = function(wsEvent) { _self.OnError(wsEvent) };
}

/*!
 * \fn void Close
 * \public
 *
 * Close the websocket connection
 */
WebSocketEventHandler.prototype.Close = function(code, message) {
    websocket.close(code, message);
}

/*!
 * \fn void Reconnect
 * \private
 *
 * Attempt reconnection
 */
WebSocketEventHandler.prototype.Reconnect = function(wsEvent) {
    console.log("WebSocketEventHandler: Attempting reconnection");
    this.Open();
}

/*!
 * \fn void UpdateFilters
 * \Private
 *
 * Update our central list of event filters from the lists provided by
 * each listener and send that list to the server so it can perform filtering
 * of unwanted events.
 */
WebSocketEventHandler.prototype.UpdateFilters = function() {
    this.filters = new Array(); // Clear filters

    // Iterate over listeners, adding their list of filters to our list,
    // avoiding duplication
    for (i = 0; i < this.listeners.length; i++) {
        var listener = this.listeners[i];
        for (j = 0; j < listener.filters.length; j++) {
            if (this.filters.indexOf(listener.filters[j]) == -1)
                this.filters.push(listener.filters[j]);
        }
    }

    // DEBUG
//     for (n = 0; n < this.filters.length; n++) {
//         console.log(n + " : " + this.filters[n]);
//     }
    var command = "WS_EVENT_SET_FILTER " + this.filters.join(' ');
    this.writeQueue.push(command); // Socket may not be connected yet so queue commands
    this.ProcessWriteQueue();
}

/*!
 * \fn void ProcessWriteQueue
 * \Private
 *
 * Sends all queued messages to the server. Since the websocket connection may
 * not have been started or is in the CONNECTING state we queue messages and
 * send them as the connection transitions to the OPEN state
 */
WebSocketEventHandler.prototype.ProcessWriteQueue = function(listener) {
    if (this.websocket.readyState != WebSocket.OPEN)
        return;

    var command;
    for (i = 0; i < this.writeQueue.length; i++) {
        command = this.writeQueue[i];
        console.log("WebSocketEventHandler: Sending > " + command);
        this.websocket.send(command);
    }

    // Clear Queue
    this.writeQueue = new Array();
}

/*!
 * \fn void AddListener
 * \public
 * \param WebSocketEventClient The client to register
 *
 * Register a WebSocketEventClient object
 */
WebSocketEventHandler.prototype.AddListener = function(listener) {
    if (typeof listener !== 'object' ||
        !listener.hasOwnProperty('eventReceiver'))
    {
        console.log("WebSocketEventHandler: Invalid listener supplied");
        return;
    }
    if (this.listeners.indexOf(listener) != -1)
    {
        console.log("WebSocketEventHandler: Duplicate listener, ignoring");
        return;
    }

    this.listeners.push(listener);
    console.log("WebSocketEventHandler: Added new listener (" + listener.name + ")");

    this.UpdateFilters();
}

/*!
 * \fn void RemoveListener
 * \public
 * \param WebSocketEventClient The client to unregister
 *
 * Deregister a WebSocketEventClient object
 */
WebSocketEventHandler.prototype.RemoveListener = function(listener) {
    for (i = 0; i < this.listeners.length; i++) {
        if (this.listeners[i] == listener) {
            this.listeners.splice(i, 1);
            console.log("WebSocketEventHandler: Removed listener (" + listener.name + ")");
            break;
        }
    }
}

