Overview
========

There are some samples showing PUB-SUB, REQ-REP and PUSH-PULL protocol with multi-part messages in action. As nzmqt uses C++ exceptions for error handling you will need to catch them by overriding QCoreApplication::notify() method. The included samples will show you how this can be done.

Running samples
---------------

All samples are compiled into one executable called 'nzmqt'. You can try them out by providing appropriate command line parameters. If you don't provide any, or if you provide '-h' or '--help' option, a list of all examples together with required parameters will be shown:

```
USAGE: ./nzmqt [-h|--help]                                                                 -- Show this help message.

USAGE: ./nzmqt pubsub-server <address> <topic>                                             -- Start PUB server.
       ./nzmqt pubsub-client <address> <topic>                                             -- Start SUB client.

USAGE: ./nzmqt reqrep-server <address> <reply-msg>                                         -- Start REQ server.
       ./nzmqt reqrep-client <address> <request-msg>                                       -- Start REP client.

USAGE: ./nzmqt pushpull-ventilator <ventilator-address> <sink-address> <numberOfWorkItems> -- Start ventilator.
       ./nzmqt pushpull-worker <ventilator-address> <sink-address>                         -- Start a worker.
       ./nzmqt pushpull-sink <sink-address>                                                -- Start sink.

Publish-Subscribe Sample:
* Server: ./nzmqt pubsub-server tcp://127.0.0.1:1234 ping
* Client: ./nzmqt pubsub-client tcp://127.0.0.1:1234 ping

Request-Reply Sample:
* Server: ./nzmqt reqrep-server tcp://127.0.0.1:1234 World
* Client: ./nzmqt reqrep-client tcp://127.0.0.1:1234 Hello

Push-Pull Sample:
* Ventilator:  ./nzmqt pushpull-ventilator tcp://127.0.0.1:5557 tcp://127.0.0.1:5558 100
* Worker 1..n: ./nzmqt pushpull-worker tcp://127.0.0.1:5557 tcp://127.0.0.1:5558
* Sink:        ./nzmqt pushpull-sink tcp://127.0.0.1:5558
```

As can be seen, this output also provides examples showing how to run them with concrete command line parameters. You can directly copy and past them to a shell prompt and run them without changing anything (assumed the used ports are free).

Documentation
-------------

Sample-specific information can be found here:
* [pubsub][]: Demonstrates how to implement PUB-SUB protocol.
* [pushpull][]: Demonstrates how to implement PUSH-PULL protocol.
* [reqrep][]: Demonstrates how to implement REQ-REP protocol.


 [pubsub]:      Samples-pubsub.md       "PUB-SUB protocol example"
 [pushpull]:    Samples-pushpull.md     "PUSH-PULL protocol example"
 [reqrep]:      Samples-reqrep.md       "REQ-REP protocol example"
