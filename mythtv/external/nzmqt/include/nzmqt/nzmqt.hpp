// Copyright 2011-2012 Johann Duscher. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice, this list of
//       conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright notice, this list
//       of conditions and the following disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY JOHANN DUSCHER ''AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are those of the
// authors and should not be interpreted as representing official policies, either expressed
// or implied, of Johann Duscher.

#ifndef NZMQT_H
#define NZMQT_H

#include <zmq.hpp>

#include <QDebug>
#include <QObject>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <QSocketNotifier>
#include <QMetaType>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QRunnable>
#include <QFlag>

// Define default context implementation to be used.
#ifndef NZMQT_DEFAULT_ZMQCONTEXT_IMPLEMENTATION
    #define NZMQT_DEFAULT_ZMQCONTEXT_IMPLEMENTATION PollingZMQContext
    //#define NZMQT_DEFAULT_ZMQCONTEXT_IMPLEMENTATION SocketNotifierZMQContext
#endif

// Define default number of IO threads to be used by ZMQ.
#ifndef NZMQT_DEFAULT_IOTHREADS
    #define NZMQT_DEFAULT_IOTHREADS 4
#endif

// Define default poll interval for polling-based implementation.
#ifndef NZMQT_POLLINGZMQCONTEXT_DEFAULT_POLLINTERVAL
    #define NZMQT_POLLINGZMQCONTEXT_DEFAULT_POLLINTERVAL 10 /* msec */
#endif

// Declare metatypes for using them in Qt signals.
Q_DECLARE_METATYPE(QList< QList<QByteArray> >)
Q_DECLARE_METATYPE(QList<QByteArray>)

namespace nzmqt
{
    typedef zmq::free_fn free_fn;
    typedef zmq::pollitem_t pollitem_t;

    typedef zmq::error_t ZMQException;

    using zmq::poll;
    using zmq::version;

    // This class wraps ZMQ's message structure.
    class ZMQMessage : private zmq::message_t
    {
        friend class ZMQSocket;

        typedef zmq::message_t super;

    public:
        inline ZMQMessage() : super() {}

        inline ZMQMessage(size_t size_) : super(size_) {}

        inline ZMQMessage(void* data_, size_t size_, free_fn *ffn_, void* hint_ = 0)
            : super(data_, size_, ffn_, hint_) {}

        inline ZMQMessage(const QByteArray& b) : super(b.size())
        {
            memcpy(data(), b.constData(), b.size());
        }

        using super::rebuild;

        inline void move(ZMQMessage* msg_)
        {
            super::move(static_cast<zmq::message_t*>(msg_));
        }

        inline void copy(ZMQMessage* msg_)
        {
            super::copy(msg_);
        }

        inline void clone(ZMQMessage* msg_)
        {
            rebuild(msg_->size());
            memcpy(data(), msg_->data(), size());
        }

        using super::data;

        using super::size;

        inline QByteArray toByteArray()
        {
            return QByteArray((const char *)data(), size());
        }
    };

    class ZMQContext;

    // This class cannot be instantiated. Its purpose is to serve as an
    // intermediate base class that provides Qt-based convenience methods
    // to subclasses.
    class ZMQSocket : public QObject, private zmq::socket_t
    {
        Q_OBJECT
        Q_ENUMS(Type Event SendFlag ReceiveFlag Option)
        Q_FLAGS(Event Events)
        Q_FLAGS(SendFlag SendFlags)
        Q_FLAGS(ReceiveFlag ReceiveFlags)

        typedef QObject qsuper;
        typedef zmq::socket_t zmqsuper;

    public:
        enum Type
        {
            TYP_PUB = ZMQ_PUB,
            TYP_SUB = ZMQ_SUB,
            TYP_PUSH = ZMQ_PUSH,
            TYP_PULL = ZMQ_PULL,
            TYP_REQ = ZMQ_REQ,
            TYP_REP = ZMQ_REP,
            TYP_DEALER = ZMQ_DEALER,
            TYP_ROUTER = ZMQ_ROUTER,
            TYP_PAIR = ZMQ_PAIR,
            TYP_XPUB = ZMQ_XPUB,
            TYP_XSUB = ZMQ_XSUB,
        };

        enum Event
        {
            EVT_POLLIN = ZMQ_POLLIN,
            EVT_POLLOUT = ZMQ_POLLOUT,
            EVT_POLLERR = ZMQ_POLLERR,
        };
        Q_DECLARE_FLAGS(Events, Event)

        enum SendFlag
        {
            SND_MORE = ZMQ_SNDMORE,
            SND_NOBLOCK = ZMQ_NOBLOCK,
        };
        Q_DECLARE_FLAGS(SendFlags, SendFlag)

        enum ReceiveFlag
        {
            RCV_NOBLOCK = ZMQ_NOBLOCK,
        };
        Q_DECLARE_FLAGS(ReceiveFlags, ReceiveFlag)

        enum Option
        {
            // Get only.
            OPT_TYPE = ZMQ_TYPE,
            OPT_RCVMORE = ZMQ_RCVMORE,
            OPT_FD = ZMQ_FD,
            OPT_EVENTS = ZMQ_EVENTS,

            // Set only.
            OPT_SUBSCRIBE = ZMQ_SUBSCRIBE,
            OPT_UNSUBSCRIBE = ZMQ_UNSUBSCRIBE,

            // Get and set.
            OPT_HWM = ZMQ_HWM,
            OPT_SWAP = ZMQ_SWAP,
            OPT_AFFINITY = ZMQ_AFFINITY,
            OPT_IDENTITY = ZMQ_IDENTITY,
            OPT_RATE = ZMQ_RATE,
            OPT_RECOVERY_IVL = ZMQ_RECOVERY_IVL,
            OPT_RECOVERY_IVL_MSEC = ZMQ_RECOVERY_IVL_MSEC,
            OPT_MCAST_LOOP = ZMQ_MCAST_LOOP,
            OPT_SNDBUF = ZMQ_SNDBUF,
            OPT_RCVBUF = ZMQ_RCVBUF,
            OPT_LINGER = ZMQ_LINGER,
            OPT_RECONNECT_IVL = ZMQ_RECONNECT_IVL,
            OPT_RECONNECT_IVL_MAX = ZMQ_RECONNECT_IVL_MAX,
            OPT_BACKLOG = ZMQ_BACKLOG,
        };

        using zmqsuper::operator void *;

        using zmqsuper::close;

        inline void setOption(Option optName_, const void *optionVal_, size_t optionValLen_)
        {
            setsockopt(optName_, optionVal_, optionValLen_);
        }

        inline void setOption(Option optName_, const char* str_)
        {
            setOption(optName_, str_, strlen(str_));
        }

        inline void setOption(Option optName_, const QByteArray& bytes_)
        {
            setOption(optName_, bytes_.constData(), bytes_.size());
        }

        inline void setOption(Option optName_, int value_)
        {
            setOption(optName_, &value_, sizeof(value_));
        }

        inline void getOption(Option option_, void *optval_, size_t *optvallen_) const
        {
            const_cast<ZMQSocket*>(this)->getsockopt(option_, optval_, optvallen_);
        }

        inline void bindTo(const QString& addr_)
        {
            bind(addr_.toLocal8Bit());
        }

        inline void bindTo(const char *addr_)
        {
            bind(addr_);
        }

        inline void connectTo(const QString& addr_)
        {
            zmqsuper::connect(addr_.toLocal8Bit());
        }

        inline void connectTo(const char* addr_)
        {
            zmqsuper::connect(addr_);
        }

        inline bool sendMessage(ZMQMessage& msg_, SendFlags flags_ = SND_NOBLOCK)
        {
            return send(msg_, flags_);
        }

        inline bool sendMessage(const QByteArray& bytes_, SendFlags flags_ = SND_NOBLOCK)
        {
            ZMQMessage msg(bytes_);
            return send(msg, flags_);
        }

        // Interprets the provided list of byte arrays as a multi-part message
        // and sends them accordingly.
        // If an empty list is provided this method doesn't do anything and returns trua.
        inline bool sendMessage(const QList<QByteArray>& msg_, SendFlags flags_ = SND_NOBLOCK)
        {
            int i;
            for (i = 0; i < msg_.size() - 1; i++)
            {
                if (!sendMessage(msg_[i], flags_ | SND_MORE))
                    return false;
            }
            if (i < msg_.size())
                return sendMessage(msg_[i], flags_);

            return true;
        }

        // Receives a message or a message part.
        inline bool receiveMessage(ZMQMessage* msg_, ReceiveFlags flags_ = RCV_NOBLOCK)
        {
            return recv(msg_, flags_);
        }

        // Receives a message.
        // The message is represented as a list of byte arrays representing
        // a message's parts. If the message is not a multi-part message the
        // list will only contain one array.
        inline QList<QByteArray> receiveMessage()
        {
            QList<QByteArray> parts;

            ZMQMessage msg;
            while (receiveMessage(&msg))
            {
                parts += msg.toByteArray();
                msg.rebuild();

                if (!hasMoreMessageParts())
                    break;
            }

            return parts;
        }

        // Receives all messages currently available.
        // Each message is represented as a list of byte arrays representing the messages
        // and their parts in case of multi-part messages. If a message isn't a multi-part
        // message the corresponding byte array list will only contain one element.
        // Note that this method won't work with REQ-REP protocol.
        inline QList< QList<QByteArray> > receiveMessages()
        {
            QList< QList<QByteArray> > ret;

            QList<QByteArray> parts = receiveMessage();
            while (!parts.isEmpty())
            {
                ret += parts;

                parts = receiveMessage();
            }

            return ret;
        }

        inline int fileDescriptor() const
        {
            int value;
            size_t size = sizeof(value);
            getOption(OPT_FD, &value, &size);
            return value;
        }

        inline Events events() const
        {
            quint32 value;
            size_t size = sizeof(value);
            getOption(OPT_EVENTS, &value, &size);
            return static_cast<Events>(value);
        }

        // Returns true if there are more parts of a multi-part message
        // to be received.
        inline bool hasMoreMessageParts() const
        {
            quint64 value;
            size_t size = sizeof(value);
            getOption(OPT_RCVMORE, &value, &size);
            return value;
        }

        inline void setIdentity(const char* nameStr_)
        {
            setOption(OPT_IDENTITY, nameStr_);
        }

        inline void setIdentity(const QString& name_)
        {
            setOption(OPT_IDENTITY, name_.toLocal8Bit());
        }

        inline void setIdentity(const QByteArray& name_)
        {
            setOption(OPT_IDENTITY, const_cast<char*>(name_.constData()), name_.size());
        }

        inline QByteArray identity() const
        {
            char idbuf[256];
            size_t size = sizeof(idbuf);
            getOption(OPT_IDENTITY, idbuf, &size);
            return QByteArray(idbuf, size);
        }

        inline void setLinger(int msec_)
        {
            setOption(OPT_LINGER, msec_);
        }

        inline int linger() const
        {
            int msec=-1;
            size_t size = sizeof(msec);
            getOption(OPT_LINGER, &msec, &size);
            return msec;
        }

        inline void subscribeTo(const char* filterStr_)
        {
            setOption(OPT_SUBSCRIBE, filterStr_);
        }

        inline void subscribeTo(const QString& filter_)
        {
            setOption(OPT_SUBSCRIBE, filter_.toLocal8Bit());
        }

        inline void subscribeTo(const QByteArray& filter_)
        {
            setOption(OPT_SUBSCRIBE, filter_);
        }

        inline void unsubscribeFrom(const char* filterStr_)
        {
            setOption(OPT_UNSUBSCRIBE, filterStr_);
        }

        inline void unsubscribeFrom(const QString& filter_)
        {
            setOption(OPT_UNSUBSCRIBE, filter_.toLocal8Bit());
        }

        inline void unsubscribeFrom(const QByteArray& filter_)
        {
            setOption(OPT_UNSUBSCRIBE, filter_);
        }

    protected:
        ZMQSocket(ZMQContext* context_, Type type_);
    };
    Q_DECLARE_OPERATORS_FOR_FLAGS(ZMQSocket::Events)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ZMQSocket::SendFlags)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ZMQSocket::ReceiveFlags)


    // This class is an abstract base class for concrete implementations.
    class ZMQContext : public QObject, private zmq::context_t
    {
        Q_OBJECT

        typedef QObject qsuper;
        typedef zmq::context_t zmqsuper;

        friend class ZMQSocket;

    public:
        inline ZMQContext(QObject* parent_ = 0, int io_threads_ = NZMQT_DEFAULT_IOTHREADS)
            : qsuper(parent_), zmqsuper(io_threads_) {}

        // Deleting children is necessary, because otherwise the children are deleted after the context
        // which results in a blocking state. So we delete the children before the zmq::context_t
        // destructor implementation is called.
        inline ~ZMQContext()
        {
            QObjectList children_ = children();
            foreach (QObject* child, children_)
                delete child;
        }

        using zmqsuper::operator void*;

        // Creates a socket instance of the specified type.
        // The created instance will have this context set as its parent,
        // so deleting this context will first delete the socket.
        // You can call 'ZMQSocket::setParent()' method to change ownership,
        // but then you need to make sure the socket instance is deleted
        // before its context. Otherwise, you might encounter blocking
        // behavior.
        inline ZMQSocket* createSocket(ZMQSocket::Type type_)
        {
            return createSocket(type_, this);
        }

        // Creates a socket instance of the specified type and parent.
        // The created instance will have the specified parent.
        // You can also call 'ZMQSocket::setParent()' method to change
        // ownership later on, but then you need to make sure the socket
        // instance is deleted before its context. Otherwise, you might
        // encounter blocking behavior.
        inline ZMQSocket* createSocket(ZMQSocket::Type type_, QObject* parent_)
        {
            ZMQSocket* socket = createSocketInternal(type_);
            socket->setParent(parent_);
            return socket;
        }

        // Start watching for incoming messages.
        virtual void start() = 0;

        // Stop watching for incoming messages.
        virtual void stop() = 0;

        // Indicates if watching for incoming messages is enabled.
        virtual bool isStopped() const = 0;

    protected:
        // Creates a socket instance of the specified type.
        virtual ZMQSocket* createSocketInternal(ZMQSocket::Type type_) = 0;
    };


    inline ZMQSocket::ZMQSocket(ZMQContext* context_, Type type_)
        : qsuper(0), zmqsuper(*context_, type_)
    {
    }


    class ZMQDevice : public QObject, public QRunnable
    {
        Q_OBJECT
        Q_ENUMS(Type)

    public:
        enum Type
        {
            TYP_QUEUE = ZMQ_QUEUE,
            TYP_FORWARDED = ZMQ_FORWARDER,
            TYP_STREAMER = ZMQ_STREAMER,
        };

        inline ZMQDevice(Type type, ZMQSocket* frontend, ZMQSocket* backend)
            : type_(type), frontend_(frontend), backend_(backend)
        {
        }

        inline void run()
        {
            zmq::device(type_, *frontend_, *backend_);
        }

    private:
        Type type_;
        ZMQSocket* frontend_;
        ZMQSocket* backend_;
    };


    // An instance of this class cannot directly be created. Use one
    // of the 'PollingZMQContext::createSocket()' factory methods instead.
    class PollingZMQSocket : public ZMQSocket
    {
        Q_OBJECT

        typedef ZMQSocket super;

        friend class PollingZMQContext;

    protected:
        inline PollingZMQSocket(ZMQContext* context_, Type type_)
            : super(context_, type_) {}

        // This method is called by the socket's context object in order
        // to signal a new received message.
        inline void onMessageReceived(const QList<QByteArray>& message)
        {
            emit messageReceived(message);
        }

    signals:
        void messageReceived(const QList<QByteArray>&);
    };

    class PollingZMQContext : public ZMQContext, public QRunnable
    {
        Q_OBJECT

        typedef ZMQContext super;

    public:
        inline PollingZMQContext(QObject* parent_ = 0, int io_threads_ = NZMQT_DEFAULT_IOTHREADS)
            : super(parent_, io_threads_),
              m_pollItemsMutex(QMutex::Recursive),
              m_interval(NZMQT_POLLINGZMQCONTEXT_DEFAULT_POLLINTERVAL),
              m_stopped(false)
        {
            setAutoDelete(false);
        }

        inline virtual ~PollingZMQContext()
        {
            QMutexLocker lock(&m_pollItemsMutex);

            Sockets list = m_sockets;
            for (Sockets::iterator soIt = list.begin(); soIt != list.end(); soIt++)
                unregisterSocket(*soIt);
        }

        // Sets the polling interval.
        // Note that the interval does not denote the time the zmq::poll() function will
        // block in order to wait for incoming messages. Instead, it denotes the time in-between
        // consecutive zmq::poll() calls.
        inline void setInterval(int interval_)
        {
            m_interval = interval_;
        }

        inline int getInterval() const
        {
            return m_interval;
        }

        // Starts the polling process by scheduling a call to the 'run()' method into Qt's event loop.
        inline void start()
        {
            m_stopped = false;
            QTimer::singleShot(0, this, SLOT(run()));
        }

        // Stops the polling process in the sense that no further 'run()' calls will be scheduled into
        // Qt's event loop.
        inline void stop()
        {
            m_stopped = true;
        }

        inline bool isStopped() const
        {
            return m_stopped;
        }

    public slots:
        // If the polling process is not stopped (by a previous call to the 'stop()' method) this
        // method will call the 'poll()' method once and re-schedule a subsequent call to this method
        // using the current polling interval.
        inline void run()
        {
            if (m_stopped)
                return;

            try {
                poll();
            } catch (ZMQException e) {
		qWarning("Exception during poll: %s\n", e.what());
            }

            if (!m_stopped)
                QTimer::singleShot(m_interval, this, SLOT(run()));
        }

        // This method will poll on all currently available poll-items (known ZMQ sockets)
        // using the given timeout to wait for incoming messages. Note that this timeout has
        // nothing to do with the polling interval. Instead, the poll method will block the current
        // thread by waiting at most the specified amount of time for incoming messages.
        // This method is public because it can be called directly if you need to.
        inline void poll(long timeout_ = 0)
        {
            int cnt;
            do {
                QMutexLocker lock(&m_pollItemsMutex);

                if (m_pollItems.empty())
                    return;

                cnt = zmq::poll(&m_pollItems[0], m_pollItems.size(), timeout_);
                if (cnt <= 0)
                    return;

                PollItems::iterator poIt = m_pollItems.begin();
                Sockets::iterator soIt = m_sockets.begin();
                int i = 0;
                while (i < cnt && poIt != m_pollItems.end())
                {
                    if (poIt->revents & ZMQSocket::EVT_POLLIN)
                    {
                        QList<QByteArray> message = (*soIt)->receiveMessage();
                        (*soIt)->onMessageReceived(message);
                        i++;
                    }
                    ++soIt;
                    ++poIt;
                }
            } while (cnt > 0);
        }

    protected:
        inline PollingZMQSocket* createSocketInternal(ZMQSocket::Type type_)
        {
            PollingZMQSocket* socket = new PollingZMQSocket(this, type_);
            // Add the socket to the poll-item list.
            registerSocket(socket);
            return socket;
        }

        // Add the given socket to list list of poll-items.
        inline void registerSocket(PollingZMQSocket* socket_)
        {
            // Make sure the socket is removed from the poll-item list as soon
            // as it is destroyed.
            connect(socket_, SIGNAL(destroyed(QObject*)), SLOT(unregisterSocket(QObject*)));

            pollitem_t pollItem = { *socket_, 0, ZMQSocket::EVT_POLLIN, 0 };

            QMutexLocker lock(&m_pollItemsMutex);
            m_sockets.push_back(socket_);
            m_pollItems.push_back(pollItem);
        }

    protected slots:
        // Remove the given socket object from the list of poll-items.
        inline void unregisterSocket(QObject* socket_)
        {
            QMutexLocker lock(&m_pollItemsMutex);

            PollItems::iterator poIt = m_pollItems.begin();
            Sockets::iterator soIt = m_sockets.begin();
            while (soIt != m_sockets.end())
            {
                if (*soIt == socket_)
                {
                    socket_->disconnect(this);
                    m_sockets.erase(soIt);
                    m_pollItems.erase(poIt);
                    break;
                }
                ++soIt;
                ++poIt;
            }
        }

    private:
        typedef QVector<pollitem_t> PollItems;
        typedef QVector<PollingZMQSocket*> Sockets;

        Sockets m_sockets;
        PollItems m_pollItems;
        QMutex m_pollItemsMutex;
        int m_interval;
        volatile bool m_stopped;
    };


    // An instance of this class cannot directly be created. Use one
    // of the 'SocketNotifierZMQContext::createSocket()' factory methods instead.
    class SocketNotifierZMQSocket : public ZMQSocket
    {
        Q_OBJECT

        friend class SocketNotifierZMQContext;

        typedef ZMQSocket super;

//    public:
//        using super::sendMessage;

//        inline bool sendMessage(const QByteArray& bytes_, SendFlags flags_ = SND_NOBLOCK)
//        {
//            bool result = super::sendMessage(bytes_, flags_);

//            if (!result)
//                socketNotifyWrite_->setEnabled(true);

//            return result;
//        }

    protected:
        inline SocketNotifierZMQSocket(ZMQContext* context_, Type type_)
            : super(context_, type_),
              socketNotifyRead_(0)
//              socketNotifyWrite_(0)
        {
            int fd = fileDescriptor();

            socketNotifyRead_ = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            QObject::connect(socketNotifyRead_, SIGNAL(activated(int)), this, SLOT(socketReadActivity()));

//            socketNotifyWrite_ = new QSocketNotifier(fd, QSocketNotifier::Write, this);
//            socketNotifyWrite_->setEnabled(false);
//            QObject::connect(socketNotifyWrite_, SIGNAL(activated(int)), this, SLOT(socketWriteActivity()));
        }

    protected slots:
        inline void socketReadActivity()
        {
            socketNotifyRead_->setEnabled(false);

            while(events() & EVT_POLLIN)
            {
                QList<QByteArray> message = receiveMessage();
                emit messageReceived(message);
            }

            socketNotifyRead_->setEnabled(true);
        }

//        inline void socketWriteActivity()
//        {
//            if(events() == 0)
//            {
//                socketNotifyWrite_->setEnabled(false);
//            }
//        }

    signals:
        void messageReceived(const QList<QByteArray>&);

    private:
        QSocketNotifier *socketNotifyRead_;
//        QSocketNotifier *socketNotifyWrite_;
    };

    class SocketNotifierZMQContext : public ZMQContext
    {
        Q_OBJECT

        typedef ZMQContext super;

    public:
        inline SocketNotifierZMQContext(QObject* parent_ = 0, int io_threads_ = NZMQT_DEFAULT_IOTHREADS)
            : super(parent_, io_threads_)
        {
        }

        inline void start()
        {
        }

        inline void stop()
        {
        }

        inline bool isStopped() const
        {
            return false;
        }

    protected:
        inline SocketNotifierZMQSocket* createSocketInternal(ZMQSocket::Type type_)
        {
            return new SocketNotifierZMQSocket(this, type_);
        }
    };

    inline ZMQContext* createDefaultContext(QObject* parent_ = 0, int io_threads_ = NZMQT_DEFAULT_IOTHREADS)
    {
        return new NZMQT_DEFAULT_ZMQCONTEXT_IMPLEMENTATION(parent_, io_threads_);
    }
}


#endif // NZMQT_H
