/*
	discoverythread.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Little thread that keeps looking for (dis)appearance of mfd's

*/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
using namespace std;

#include <qapplication.h>

#include "discoverythread.h"
#include "events.h"

//
//  C callback from mdnsd stuff
//

extern "C" int callbackAnswer(mdnsda a, void *arg)
{
    DiscoveryThread *discovery_thread = (DiscoveryThread *)arg;
    
    discovery_thread->handleMdnsdCallback(a);
    
    return 0;
}




DiscoveryThread::DiscoveryThread(MfdInterface *the_interface)
{
    //
    //  Get a pointer to the main MfdInterface objects so we can send it
    //  events when mfd's appear or disappear
    //
    
    mfd_interface = the_interface;

    //
    //  These get initialized in run()
    //
    
    mdns_daemon = NULL;
    mdns_ipaddr_daemon = NULL;
    

    //
    //  By default, run
    //

    keep_going = true;

    //
    //  Create a u shaped pipe so others can wake us out of a select
    //
    

    if(pipe(u_shaped_pipe) < 0)
    {
        warning("could not create a u shaped pipe");
    }
    
    //
    //  Create our (empty) list of discovered mfd's
    //
    
    discovered_mfds = new QPtrList<DiscoveredMfd>;
    discovered_mfds->setAutoDelete(true);
    
}

void DiscoveryThread::run()
{

    //
    //  A lot of this is based on the mquery example in mdnsd
    //

    struct message m;
    unsigned long int ip;
    unsigned short int port;
    int bsize, ssize = sizeof(struct sockaddr_in);
    unsigned char buf[MAX_PACKET_LEN];
    struct sockaddr_in from, to;


    //
    //  Create the main mdsnd process. I have no idea what either of these
    //  parameters are supposed to mean.
    //

    mdns_daemon = mdnsd_new(1,1000);
    
    //
    //  Create a separate one just to do IP address lookups. Why? Well, if
    //  you try to do IP lookups with just the "main" mdns_daemon, there
    //  always seem to be timing issues (ie. doesn't work properly)
    //
    
    mdns_ipaddr_daemon = mdnsd_new(1, 1000);


    //
    //  Get a multicast socket for the main mdns daemon
    //
    

    int multicast_socket = createMulticastSocket();
    
    if( multicast_socket < 1)
    {
        mdnsd_shutdown(mdns_daemon);
        mdnsd_free(mdns_daemon);
        mdns_daemon = NULL;
        cerr << "discoverythread.o: could not create multicast socket" << endl;
        return;
    }
    
    //
    //  Get a multicast socket for the ip address mdns daemon
    //
    

    int ipaddr_socket = createMulticastSocket();
    
    if( ipaddr_socket < 1)
    {
        mdnsd_shutdown(mdns_daemon);
        mdnsd_free(mdns_daemon);
        mdns_daemon = NULL;

        mdnsd_shutdown(mdns_ipaddr_daemon);
        mdnsd_free(mdns_ipaddr_daemon);
        mdns_ipaddr_daemon = NULL;

        cerr << "discoverythread.o: could not create multicast socket" << endl;
        return;
    }
    
    //
    //  We watch only for mfdp services, cause once we've found an mfd, we
    //  can query it directly about what services it has
    //

    mdnsd_query(mdns_daemon,"_mfdp._tcp.local.", QTYPE_PTR, callbackAnswer, this);

    //
    //  Go to work
    //

    while(keep_going)
    {
        fd_set fds;
        int nfds = 0;
        FD_ZERO(&fds);

        //
        //  Set up for select() to watch the multicast socket
        //

        FD_SET(multicast_socket,&fds);
        if(nfds <= multicast_socket)
        {
            nfds = multicast_socket + 1;
        }

        //
        //  Set up for select() to watch the multicast ipaddress socket
        //

        FD_SET(ipaddr_socket,&fds);
        if(nfds <= ipaddr_socket)
        {
            nfds = ipaddr_socket + 1;
        }

        //
        //  Add every mfd we are already connected to
        //

        QPtrListIterator<DiscoveredMfd> it( *discovered_mfds );
        DiscoveredMfd *an_mfd;
        while ( (an_mfd = it.current()) != 0 )
        {
            ++it;
            if(an_mfd->isIpResolved())
            {
                int mfd_socket = an_mfd->getSocket();
                if(mfd_socket > 0)
                {
                    FD_SET(mfd_socket,&fds);
                    if(nfds <= mfd_socket)
                    {
                        nfds = mfd_socket + 1;
                    }
                }
                else
                {
                    cerr << "discoverythread.o: discovered mfd object said "
                         << "it was resolved, but has bad socket"
                         << endl;
                }
            }
        }

        //
        //  Add the control pipe
        //
            
        FD_SET(u_shaped_pipe[0], &fds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    
    
        //
        //  Sleep as long as mdnsd tells us to
        //

        struct timeval *tv;
        tv = mdnsd_sleep(mdns_daemon);
        
        struct timeval *another_tv;
        another_tv = mdnsd_sleep(mdns_ipaddr_daemon);
        
        //
        //  Sit in select until something happens
        //
        
        int result = 0;
        if(another_tv->tv_sec * 1000 + another_tv->tv_usec < tv->tv_sec * 1000 + tv->tv_usec)
        {
            result = select(nfds,&fds,NULL,NULL,another_tv);
        }
        else
        {
            result = select(nfds,&fds,NULL,NULL,tv);
        }

        if(result < 0)
        {
            cerr << "discoverythread.o: got an error on select (?), "
                 << "which is far from a good thing" 
                 << endl;
        }
    
        //
        //  In case data came in on out u_shaped_pipe, clean it out
        //

        if(FD_ISSET(u_shaped_pipe[0], &fds))
        {
            u_shaped_pipe_mutex.lock();
                char read_back[2049];
                read(u_shaped_pipe[0], read_back, 2048);
            u_shaped_pipe_mutex.unlock();
        }
        
        //
        //  If something came in any of our mfd sockets, clean them out
        //
        
        QPtrListIterator<DiscoveredMfd> clean_it( *discovered_mfds );
        DiscoveredMfd *a_mfd;
        while ( (a_mfd = clean_it.current()) != 0 )
        {
            ++clean_it;
            if(a_mfd->isIpResolved())
            {
                int mfd_socket = a_mfd->getSocket();
                if(mfd_socket > 0)
                {
                    if(FD_ISSET(mfd_socket, &fds))
                    {
                        char read_back[2049];
                        read(mfd_socket, read_back, 2048);
                    }
                }
            }
        }

        
        
        if(FD_ISSET(multicast_socket,&fds))
        {
            while((bsize = recvfrom(
                                    multicast_socket,
                                    buf,
                                    MAX_PACKET_LEN,
                                    0,
                                    (struct sockaddr*)&from, 
                                    (socklen_t *)&ssize)
                                   ) > 0)
            {
                bzero(&m,sizeof(struct message));
                message_parse(&m,buf);
                mdnsd_in(
                            mdns_daemon,
                            &m,
                            (unsigned int)from.sin_addr.s_addr,
                            from.sin_port
                        );
            }
            if(bsize < 0 && errno != EAGAIN) 
            { 
                printf("can't read socket %d: %s\n",errno,strerror(errno)); 
                //return 1; 
            }
        }
        while(mdnsd_out(mdns_daemon,&m,&ip,&port))
        {
            bzero(&to, sizeof(to));
            to.sin_family = AF_INET;
            to.sin_port = port;
            to.sin_addr.s_addr = ip;
            if(
                    sendto(
                            multicast_socket,
                            message_packet(&m),
                            message_packet_len(&m),
                            0,
                            (struct sockaddr *)&to,
                            sizeof(struct sockaddr_in)
                           ) 
                           != message_packet_len(&m)
              )
            { 
                printf("can't write to socket: %s\n",strerror(errno)); 
                //return 1; 
            }
        }
        
        if(FD_ISSET(ipaddr_socket,&fds))
        {
            while((bsize = recvfrom(
                                    ipaddr_socket,
                                    buf,
                                    MAX_PACKET_LEN,
                                    0,
                                    (struct sockaddr*)&from, 
                                    (socklen_t *)&ssize)
                                   ) > 0)
            {
                bzero(&m,sizeof(struct message));
                message_parse(&m,buf);
                mdnsd_in(
                            mdns_ipaddr_daemon,
                            &m,
                            (unsigned int)from.sin_addr.s_addr,
                            from.sin_port
                        );
            }
            if(bsize < 0 && errno != EAGAIN) 
            { 
                printf("can't read socket %d: %s\n",errno,strerror(errno)); 
                //return 1; 
            }
        }
        while(mdnsd_out(mdns_ipaddr_daemon,&m,&ip,&port))
        {
            bzero(&to, sizeof(to));
            to.sin_family = AF_INET;
            to.sin_port = port;
            to.sin_addr.s_addr = ip;
            if(
                    sendto(
                            ipaddr_socket,
                            message_packet(&m),
                            message_packet_len(&m),
                            0,
                            (struct sockaddr *)&to,
                            sizeof(struct sockaddr_in)
                           ) 
                           != message_packet_len(&m)
              )
            { 
                printf("can't write to socket: %s\n",strerror(errno)); 
                //return 1; 
            }
        }
        
        
        //
        //  Check if any of our mfd's went away without removing themselves
        //  from mDNS (ie. someone hit ctrl-c)
        //

        cleanDeadMfds();
    }

    //
    //  Free up the mdnsd stuff
    //

    mdnsd_shutdown(mdns_daemon);
    mdnsd_free(mdns_daemon);
    mdns_daemon = NULL;

    mdnsd_shutdown(mdns_ipaddr_daemon);
    mdnsd_free(mdns_ipaddr_daemon);
    mdns_ipaddr_daemon = NULL;

    close(multicast_socket);
    close(ipaddr_socket);
}

int DiscoveryThread::createMulticastSocket()
{
    int s, flag = 1, ittl = 255;
    struct sockaddr_in in;
    struct ip_mreq mc;
    char ttl = 255;

    bzero(&in, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_port = htons(5353);
    in.sin_addr.s_addr = 0;

    if((s = socket(AF_INET,SOCK_DGRAM,0)) < 0)
    {
        return 0;
    }

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag));
    if(bind(s,(struct sockaddr*)&in,sizeof(in))) { close(s); return 0; }

    mc.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mc.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc, sizeof(mc)); 
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ittl, sizeof(ittl));

    flag =  fcntl(s, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(s, F_SETFL, flag);

    return s;
    
}

void DiscoveryThread::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    wakeUp();
}

void DiscoveryThread::wakeUp()
{
    //
    //  Tell the main thread to wake up by sending some data to ourselves on
    //  our u_shaped_pipe. This may seem odd. It isn't.
    //

    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
}

void DiscoveryThread::handleMdnsdCallback(mdnsda answer)
{
    int now;
    
    if(answer->ttl == 0)
    {
        now = 0;
    }
    else
    {
        now = answer->ttl - time(0);
    }

    if(answer->type == QTYPE_PTR)
    {
        //
        //  QTYPE_PTR answers tell us about the appearance and disapperance
        //  of basic services without those services being resolved to
        //  IP:port pairs.
        //        

        
        //
        //  See if we already have data about this service
        //

        QPtrListIterator<DiscoveredMfd> it( *discovered_mfds );
        DiscoveredMfd *which_one = NULL;
        DiscoveredMfd *an_mfd;
        while ( (an_mfd = it.current()) != 0 )
        {
            ++it;
            if(an_mfd->getFullServiceName() == QString((char *)answer->rdname))
            {
                which_one = an_mfd;   
                break;
            }
        }

        if(which_one)
        {
            which_one->setTimeToLive(now);
        }
        else if(now > 0)
        {
            DiscoveredMfd *new_discovered_mfd 
                = new DiscoveredMfd(QString( (char *)answer->rdname));
            new_discovered_mfd->setTimeToLive(now);
                
            discovered_mfds->append(new_discovered_mfd);
            
            //
            //  Since this is new, we want to send out a query to get the
            //  resolved ip and port of this service
            //            

            if(mdns_daemon)
            {
                mdnsd_query(    
                            mdns_daemon,
                            (char *)answer->rdname, 
                            QTYPE_SRV, 
                            callbackAnswer, 
                            this
                           );

            }
            else
            {
                cerr << "discoverythread.o: how the hell did you get "
                     << "here without an mdnds_daemon ?"
                     << endl;
            }
        }
            
        //
        //  Anything with a time to live of 0 is dead. 
        //
        
        QPtrListIterator<DiscoveredMfd> dead_it( *discovered_mfds );
        DiscoveredMfd *dead_mfd = NULL;
        while ( (dead_mfd = dead_it.current()) != 0 )
        {
            if(dead_mfd->getTimeToLive()  < 1 && dead_mfd->isIpResolved())
            {
                MfdDiscoveryEvent *de = new
                MfdDiscoveryEvent(
                                    false,
                                    dead_mfd->getFullServiceName(), 
                                    dead_mfd->getHostName(),
                                    dead_mfd->getAddress(),
                                    dead_mfd->getPort()
                                 );
                QApplication::postEvent( mfd_interface, de );
                discovered_mfds->remove(dead_mfd);
            }
            else
            {
                ++dead_it;
            }
        }
        

    }
    else if(answer->type == QTYPE_SRV)
    {
    
        //
        //  A QTYPE_SRV answer is giving us resolved host name and port for
        //  a service (but not yet an ip address)
        //

        //
        //  Find the service in question
        //

        QPtrListIterator<DiscoveredMfd> it( *discovered_mfds );
        DiscoveredMfd *which_one = NULL;
        DiscoveredMfd *an_mfd;
        while ( (an_mfd = it.current()) != 0 )
        {
            ++it;
            if(an_mfd->getFullServiceName() == QString((char *)answer->name))
            {
                which_one = an_mfd;   
                break;
            }
        }
        if(which_one)
        {
            if(!which_one->isPortResolved())
            {
                which_one->setHostName(QString((char *) answer->rdname).section('.',0,0));
                which_one->setPort(answer->srv.port);
                which_one->isPortResolved(true);
            }
            if(!which_one->isIpResolved())
            {
                //
                //  Ask mdsnd for the actual ip address for this hostname
                //  (no, we can't just ask DNS)
                //

                QString ip_address_question = QString("%1.local.")
                                              .arg(which_one->getHostName());


                mdnsd_query(    
                            mdns_ipaddr_daemon,
                            (char *) ip_address_question.ascii(), 
                            QTYPE_A, 
                            callbackAnswer, 
                            this
                           );
            }
        }
        else
        {
            cerr << "discoverythread.o: got an mdns QTYPE_SRV response that I never asked for"
                 << endl;
        }
    }
    else if(answer->type == QTYPE_A)
    {
        in_addr address;
        address.s_addr = answer->ip;
        
        //
        //  For some obscure "let's not conflict with something" sort of
        //  reason (I think), ip address comes in backwards.
        //

        QString backwards_dot_quad = QString(inet_ntoa(address));
        
        QString ip1 = backwards_dot_quad.section(".", 3, 3);
        QString ip2 = backwards_dot_quad.section(".", 2, 2);
        QString ip3 = backwards_dot_quad.section(".", 1, 1);
        QString ip4 = backwards_dot_quad.section(".", 0, 0);

        QString dot_quad_address = QString("%1.%2.%3.%4")
                                           .arg(ip1)
                                           .arg(ip2)
                                           .arg(ip3)
                                           .arg(ip4);


        QString hostname = QString((char *)answer->name).section(".", 0, 0);

        //
        //  See if any of my discovered mfd objects are waiting for an ip
        //  address
        //

        QPtrListIterator<DiscoveredMfd> it( *discovered_mfds );
        DiscoveredMfd *which_one = NULL;
        DiscoveredMfd *an_mfd;
        while ( (an_mfd = it.current()) != 0 )
        {
            ++it;

            if(an_mfd->getHostName() == QString((char *)answer->name).section(".", 0, 0))
            {
                which_one = an_mfd;   
                break;
            }
        }
        if(which_one)
        {
            if(which_one->isPortResolved() && !which_one->isIpResolved())
            {
                which_one->setAddress(dot_quad_address);
                which_one->isIpResolved(true);

                //
                //  Now we can try to connect
                //

                if(which_one->connect())
                {
                    MfdDiscoveryEvent *de = new
                    MfdDiscoveryEvent(
                                        true, 
                                        which_one->getFullServiceName(),
                                        which_one->getHostName(),
                                        which_one->getAddress(),
                                        which_one->getPort()
                                     );
                    QApplication::postEvent(mfd_interface, de);
                }
                else
                {
                    cerr << "discoverythread.o: found an mfd but could "
                         << "not connect to it" 
                         << endl;
                    discovered_mfds->remove(which_one);
                }
            }                
        }
        else
        {
            cerr << "discoverythread.cpp: getting mdns host->ip answers that I "
                 << "never asked for"
                 << endl;
        }
    }
    else
    {
        cerr << "discoverythread.o: gettting mdnsd answer types I don't understand"
             << endl;
    }

    // answer = mdnsd_list();
}

void DiscoveryThread::cleanDeadMfds()
{
    //
    //  This method has nothing to do with zeroconfig, it just looks to see
    //  if the client to mfd has gone away (ie. mfd got ctrl-c's,
    //  segfaulted, etc.)
    //
    
    QPtrListIterator<DiscoveredMfd> it( *discovered_mfds );
    DiscoveredMfd *an_mfd = NULL;
    while ( (an_mfd = it.current()) != 0 )
    {
        if(an_mfd->isIpResolved())
        {
            if(an_mfd->getSocket() < 1)
            {
                //
                //
                //

                MfdDiscoveryEvent *de = new
                MfdDiscoveryEvent(
                                    false, 
                                    an_mfd->getFullServiceName(),
                                    an_mfd->getHostName(),
                                    an_mfd->getAddress(),
                                    an_mfd->getPort()
                                 );
                QApplication::postEvent(mfd_interface, de);
                discovered_mfds->remove(it);
            }
            else
            {
                bool still_there = true;
                QSocketDevice *the_socket = an_mfd->getSocketDevice();
                if(the_socket)
                {
                    if(the_socket->waitForMore(30, &still_there) < 1)
                    {
                        if(!still_there)
                        {
                            MfdDiscoveryEvent *de = new
                            MfdDiscoveryEvent(
                                                false, 
                                                an_mfd->getFullServiceName(),
                                                an_mfd->getHostName(),
                                                an_mfd->getAddress(),
                                                an_mfd->getPort()
                                             );
                            QApplication::postEvent(mfd_interface, de);
                            discovered_mfds->remove(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                    else
                    {
                        ++it;
                    }
                }
                else
                {
                    cerr << "discoverythread.o: oh crap, "
                         << "logical is disintegrating"
                         << endl;
                }
            }
        }
        else
        {
            ++it;
        }
    }
    
}

DiscoveryThread::~DiscoveryThread()
{
    if(discovered_mfds)
    {
        discovered_mfds->clear();
        delete discovered_mfds;
        discovered_mfds = NULL;
    }
}

 
