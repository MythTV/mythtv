nzmqt - A lightweight C++ Qt binding for 0MQ
============================================

[nzmqt][] is a lightweight C++ [Qt][] binding for [0MQ / zeromq][zeromq]. The primary goal of this project is to provide a Qt-ish interface to 0MQ intelligent transport layer library and to integrate it into Qt's event loop seamlessly.

Why use nzmqt
-------------

If you use Qt framework for platform independent software development and you want to use 0MQ transport layer library you are facing the problem to integrate message queue into Qt's event system somehow. There is already a C++ Qt binding [zeromqt][] which approaches this problem by integrating 0MQ into the Qt's event loop, mapping 0MQ message events onto Qt signals. Furthermore, it provides a Qt-like API which allows to represent messages as QByteArray instances. This is a nice idea! However, there are some drawbacks with the project's code and implementation, which is why I decided to start this project.

nzmqt is a re-implementation of the approach taken by [zeromqt][]. I took the original implementation as a source of information, but I've done a completely new implementation. I could have built upon the exiting code by just forking it, but I wanted to be sure I can use the code in my projects without problems, because until now zeromqt's author hasn't officially released his work under a certain (open source) license. Consequently, nzmqt is released to the public under the simplified BSD license. So you can use the code in your (commercial) projects without any fear. Another short coming with zeromqt is that it works fine for 0MQ non-multi-part messages, but it doesn't support multi-part messages yet. Also, it duplicates a lot of code of [0MQ's standard C++ binding][cppzmq] and, as a result, cannot profit from code improvements and bugfixes without copying or incorporating them into zeromqt's implementation. So in contrast to the original implementation, nzmqt reuses as much code of 0MQ's original C++ binding as possible by using inheritance. Additionally, several other things have been changed from an API user's perspective. In summary, nzmqt contains the following changes and improvements compared to zeromqt:

* The implementation is a complete re-write in the sense that it doesn't duplicate code of 0MQ's official C++ binding anymore. Instead, it builds upon existing code through inheritance and, hence, will likely benefit from future bugfixes and enhancements targeted at 0MQ's C++ binding.
* All classes are placed into a separate namespace 'nzmqt'.
* 0MQ's multi-part messages are supported.
* The initial support for using Qt's way of handling errors using error codes has been dropped. Instead, this code only throws exceptions originally thrown by 0MQ's official C++ API.
* As with 0MQ's C++ binding all classes are contained within a singe header file which makes integrating this Qt binding very easy.
* There is no 'ZmqContext' singleton anymore. Instead you can create your own instance of a concrete subclass of 'ZMQContext' yourself.
* The socket class 'ZMQSocket' now also inherits from QObject, so you can add it as a child to any QObject parent as you know it from Qt.
* The code is officially licensed under the (commercial-friendly) simplified BSD license.
* Not only PUB-SUB, but also REQ-REP and PUSH-PULL are supported.

More Information
----------------

* [changelog][]: What's new? Read about changes introduced with each release.
* [nzmqt issue tracker][]: Find out about the current project status, or issue bug reports and feature requests.
* [software overview][]: How to get started, API reference documentation, etc.
* [samples overview][]: Explanation of the provided examples.


 [cppzmq]:              https://github.com/zeromq/zeromq2-x/blob/master/include/zmq.hpp "C++ binding for 0MQ on GitHub"
 [nzmqt]:               https://github.com/jonnydee/nzmqt                           "nzmqt project on GitHub"
 [nzmqt issue tracker]: https://github.com/jonnydee/nzmqt/issues                    "nzmqt issue tracker on GitHub"
 [Qt]:                  http://qt-project.org/                                      "Qt project homepage"
 [zeromq]:              http://www.zeromq.org/                                      "0MQ project homepage"
 [zeromqt]:             https://github.com/wttw/zeromqt                             "zeromqt project on GitHub"

 [changelog]:           CHANGELOG.md                                                "nzmqt software changelog"
 [samples overview]:    doc/Samples.md                                              "nzmqt samples overview"
 [software overview]:   doc/Software.md                                             "nzmqt software overview"
