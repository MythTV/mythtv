Release 2.2.1-dev
-----------------
* Improved documentation.
* Emit poll errors as signals (instead of ignoring them).
* Fixed gh-8: Unsafe disconnect in PollingZMQContext::unregisterSocket(QObject\* socket_).
* Fixed gh-12: warnings with -Wall (and compilation broken if -Werror).
* Feature gh-13: Support type-safe Qt 5 singals & slots connections.

Release 2.2.0
-------------
* fixed gh-5: Change PollingZMQContext::poll to empty the socket queue rather than poll once per interval.

Release 0.7
-----------
* Introduced enumeration types for several ZMQ constants for type-safety.
* Added a new polling based implementation that works for all ZMQ communication protocols.
* Dropped support for REQ-REP protocol for old 'QSocketNotifier' based implementation.
* Added some more convenience methods to 'ZMQSocket' class.
* Old and new socket implementations now emit a signal with a received message as parameter.

