/* main.cpp
 * ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#include "zmserver.h"

// default port to listen on
#define PORT 6548

// default location of zoneminders config file
#define ZM_CONFIG "/etc/zm.conf"

// Care should be taken to keep these in sync with the exit codes in
// libmythbase/exitcodes.h (which is not included here to keep this code 
// separate from mythtv libraries).
#define EXIT_OK                      0
#define EXIT_INVALID_CMDLINE         132
#define EXIT_OPENING_LOGFILE_ERROR   136  // mapped to _PERMISSIONS_ERROR
#define EXIT_DAEMONIZING_ERROR       145
#define EXIT_SOCKET_ERROR            135

using namespace std;

int main(int argc, char **argv)
{
    fd_set master;                  // master file descriptor list
    fd_set read_fds;                // temp file descriptor list for select()
    struct sockaddr_in myaddr;      // server address
    struct sockaddr_in remoteaddr;  // client address
    struct timeval timeout;         // maximum time to wait for data
    int res;                        // result from select()
    int fdmax;                      // maximum file descriptor number
    int listener;                   // listening socket descriptor
    int newfd;                      // newly accept()ed socket descriptor
    char buf[4096];                 // buffer for client data
    int nbytes;
    int yes=1;                      // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    int i;
    bool quit = false;              // quit flag

    bool debug = false;             // debug mode enabled
    bool daemon_mode = false;       // is daemon mode enabled
    int port = PORT;                // port we're listening on
    string logfile = "";            // log file
    string zmconfig = ZM_CONFIG;    // location of zoneminders config file

    //  Check command line arguments
    for (int argpos = 1; argpos < argc; ++argpos)
    {
        if (!strcmp(argv[argpos],"-d") ||
             !strcmp(argv[argpos],"--daemon"))
        {
            daemon_mode = true;
        }
        else if (!strcmp(argv[argpos],"-n") ||
                  !strcmp(argv[argpos],"--nodaemon"))
        {
            daemon_mode = false;
        }
        else if (!strcmp(argv[argpos],"-p") ||
                  !strcmp(argv[argpos],"--port"))
        {
            if (argc > argpos)
            {
                port = atoi(argv[argpos+1]);

                if (port < 1 || port > 65534)
                {
                    cout << "Bad port number: " << port << endl;
                    return EXIT_INVALID_CMDLINE;
                }
                ++argpos;
            }
            else
            {
                cout << "Missing argument to -p/--port option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(argv[argpos],"-l") ||
                  !strcmp(argv[argpos],"--logfile"))
        {
            if (argc > argpos)
            {
                logfile = argv[argpos+1];
                if (logfile[0] == '-')
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return EXIT_INVALID_CMDLINE;
                }

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -l/--logfile option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(argv[argpos],"-c") ||
                  !strcmp(argv[argpos],"--zmconfig"))
        {
            if (argc > argpos)
            {
                zmconfig = argv[argpos+1];
                if (zmconfig[0] == '-')
                {
                    cerr << "Invalid or missing argument to -c/--zmconfig option\n";
                    return EXIT_INVALID_CMDLINE;
                }

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -c/--zmconfig option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(argv[argpos],"-v") ||
                  !strcmp(argv[argpos],"--verbose"))
        {
            debug = true;
        }
        else
        {
            cerr << "Invalid argument: " << argv[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-p or --port number        A port number to listen on (default is 6548) " << endl <<
                    "-d or --daemon             Runs mythzmserver as a daemon " << endl <<
                    "-n or --nodaemon           Does not run mythzmserver as a daemon (default)" << endl <<
                    "-c or --zmconfig           Location of zoneminders config file (default is " << ZM_CONFIG << ")" << endl <<
                    "-l or --logfile filename   Writes STDERR and STDOUT messages to filename" << endl <<
                    "-v or --verbose            Prints more debug output" << endl;
            return EXIT_INVALID_CMDLINE;
        }
    }

    // set up log file
    int logfd = -1;

    if (logfile != "")
    {
        logfd = open(logfile.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0664);

        if (logfd < 0)
        {
            perror("open(logfile)");
            return EXIT_OPENING_LOGFILE_ERROR;
        }
    }

    if (logfd != -1)
    {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);

        // Close the unduplicated logfd
        if (logfd != 1 && logfd != 2)
            close(logfd);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cout << "Unable to ignore SIGPIPE\n";

    //  Switch to daemon mode?
    if (daemon_mode)
    {
        if (daemon(0, 0) < 0)
        {
            cout << "Failed to run as a daemon. Bailing out.\n";
            return EXIT_DAEMONIZING_ERROR;
        }
        cout << endl;
    }

    map<int, ZMServer*> serverList; // list of ZMServers

    // load the config
    loadZMConfig(zmconfig);

    cout << "ZM is version '" << g_zmversion << "'" << endl;

    // connect to the DB
    connectToDatabase();

    // clear the master and temp sets
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // get the listener
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return EXIT_SOCKET_ERROR;
    }

    // lose the pesky "address already in use" error message
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes,
                                                        sizeof(int)) == -1)
    {
        perror("setsockopt");
        return EXIT_SOCKET_ERROR;
    }

    // bind
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(port);
    memset(&(myaddr.sin_zero), '\0', 8);
    if (bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1)
    {
        perror("bind");
        return EXIT_SOCKET_ERROR;
    }

    // listen
    if (listen(listener, 10) == -1)
    {
        perror("listen");
        return EXIT_SOCKET_ERROR;
    }

    cout << "Listening on port: " << port << endl;

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    while (!quit)
    {
        // the maximum time select() should wait
        timeout.tv_sec = DB_CHECK_TIME;
        timeout.tv_usec = 0;

        read_fds = master; // copy it
        res = select(fdmax+1, &read_fds, NULL, NULL, &timeout);

        if (res == -1)
        {
            perror("select");
            return EXIT_SOCKET_ERROR;
        }
        else if (res == 0)
        {
            // select timed out
            // just kick the DB connection to keep it alive
            kickDatabase(debug);
            continue;
        }

        // run through the existing connections looking for data to read
        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                // we got one!!
                if (i == listener) 
                {
                    // handle new connections
                    addrlen = sizeof(remoteaddr);
                    if ((newfd = accept(listener,
                                        (struct sockaddr *) &remoteaddr,
                                                               &addrlen)) == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        // add to master set
                        FD_SET(newfd, &master);
                        if (newfd > fdmax)
                        {    // keep track of the maximum
                            fdmax = newfd;
                        }

                        // create new ZMServer and add to map
                        ZMServer *server = new ZMServer(newfd, debug);
                        serverList[newfd] = server;

                        printf("new connection from %s on socket %d\n",
                               inet_ntoa(remoteaddr.sin_addr), newfd);
                    }
                }
                else
                {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0)
                    {
                        // got error or connection closed by client
                        if (nbytes == 0)
                        {
                            // connection closed
                            printf("socket %d hung up\n", i);
                        }
                        else
                        {
                            perror("recv");
                        }

                        close(i);

                        // remove from master set
                        FD_CLR(i, &master);

                        // remove from server list
                        ZMServer *server = serverList[i];
                        if (server)
                            delete server;
                        serverList.erase(i);
                    }
                    else
                    {
                        ZMServer *server = serverList[i];
                        quit = server->processRequest(buf, nbytes);
                    }
                }
            }
        }
    }

    // cleanly remove all the ZMServer's
    for (std::map<int, ZMServer*>::iterator it = serverList.begin();
         it != serverList.end(); ++it)
    {
        delete it->second;
    }

    mysql_close(&g_dbConn);

    return EXIT_OK;
}

