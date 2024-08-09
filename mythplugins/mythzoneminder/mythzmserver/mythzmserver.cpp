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

#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "zmserver.h"

// default port to listen on
static constexpr uint16_t PORT { 6548 };

// default location of zoneminders default config file
static constexpr const char* ZM_CONFIG { "/etc/zm/zm.conf" };

// default location of zoneminders override config file
static constexpr const char* ZM_OVERRIDECONFIG { "/etc/zm/conf.d/01-system-paths.conf" };

// Care should be taken to keep these in sync with the exit codes in
// libmythbase/exitcodes.h (which is not included here to keep this code 
// separate from mythtv libraries).
enum EXIT_CODES : std::uint8_t {
    EXIT_OK                        =   0,
    EXIT_INVALID_CMDLINE           = 132,
    EXIT_OPENING_LOGFILE_ERROR     = 136, // mapped to _PERMISSIONS_ERROR
    EXIT_DAEMONIZING_ERROR         = 145,
    EXIT_SOCKET_ERROR              = 135,
    EXIT_VERSION_ERROR             = 136,
};

int main(int argc, char **argv)
{
    fd_set master;                  // master file descriptor list
    fd_set read_fds;                // temp file descriptor list for select()
    struct sockaddr_in myaddr {};   // server address
    struct sockaddr_in remoteaddr {};// client address
    int fdmax = -1;                 // maximum file descriptor number
    int listener = -1;              // listening socket descriptor
    int newfd = -1;                 // newly accept()ed socket descriptor
    std::array<char,4096> buf {};   // buffer for client data
    int yes=1;                      // for setsockopt() SO_REUSEADDR, below
    bool quit = false;              // quit flag

    bool debug = false;             // debug mode enabled
    bool daemon_mode = false;       // is daemon mode enabled
    int port = PORT;                // port we're listening on
    std::string logfile;               // log file
    std::string zmconfig = ZM_CONFIG;  // location of zoneminders default config file
    std::string zmoverideconfig = ZM_OVERRIDECONFIG;  // location of zoneminders override config file

    //  Check command line arguments
    for (int argpos = 1; argpos < argc; ++argpos)
    {
        if (strcmp(argv[argpos],"-d") == 0 ||
             strcmp(argv[argpos],"--daemon") == 0)
        {
            daemon_mode = true;
        }
        else if (strcmp(argv[argpos],"-n") == 0 ||
                  strcmp(argv[argpos],"--nodaemon") == 0)
        {
            daemon_mode = false;
        }
        else if (strcmp(argv[argpos],"-p") == 0 ||
                  strcmp(argv[argpos],"--port") == 0)
        {
            if (argc > argpos)
            {
                port = atoi(argv[argpos+1]);

                if (port < 1 || port > 65534)
                {
                    std::cout << "Bad port number: " << port << std::endl;
                    return EXIT_INVALID_CMDLINE;
                }
                ++argpos;
            }
            else
            {
                std::cout << "Missing argument to -p/--port option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (strcmp(argv[argpos],"-l") == 0 ||
                  strcmp(argv[argpos],"--logfile") == 0)
        {
            if (argc > argpos)
            {
                logfile = argv[argpos+1];
                if (logfile[0] == '-')
                {
                    std::cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return EXIT_INVALID_CMDLINE;
                }

                ++argpos;
            }
            else
            {
                std::cerr << "Missing argument to -l/--logfile option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (strcmp(argv[argpos],"-c") == 0 ||
                  strcmp(argv[argpos],"--zmconfig") == 0)
        {
            if (argc > argpos)
            {
                zmconfig = argv[argpos+1];
                if (zmconfig[0] == '-')
                {
                    std::cerr << "Invalid or missing argument to -c/--zmconfig option\n";
                    return EXIT_INVALID_CMDLINE;
                }

                ++argpos;
            }
            else
            {
                std::cerr << "Missing argument to -c/--zmconfig option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (strcmp(argv[argpos],"-o") == 0 ||
                  strcmp(argv[argpos],"--zmoverrideconfig") == 0)
        {
            if (argc > argpos)
            {
                zmconfig = argv[argpos+1];
                if (zmconfig[0] == '-')
                {
                    std::cerr << "Invalid or missing argument to -o/--zmoverrideconfig option\n";
                    return EXIT_INVALID_CMDLINE;
                }

                ++argpos;
            }
            else
            {
                std::cerr << "Missing argument to -o/--zmoverrideconfig option\n";
                return EXIT_INVALID_CMDLINE;
            }
        }
        else if (strcmp(argv[argpos],"-v") == 0 ||
                  strcmp(argv[argpos],"--verbose") == 0)
        {
            debug = true;
        }
        else
        {
            std::cerr << "Invalid argument: " << argv[argpos] << std::endl <<
                    "Valid options are: " << std::endl <<
                    "-p or --port number        A port number to listen on (default is 6548) " << std::endl <<
                    "-d or --daemon             Runs mythzmserver as a daemon " << std::endl <<
                    "-n or --nodaemon           Does not run mythzmserver as a daemon (default)" << std::endl <<
                    "-c or --zmconfig           Location of zoneminders default config file (default is " << ZM_CONFIG << ")" << std::endl <<
                    "-o or --zmoverrideconfig   Location of zoneminders override config file (default is " << ZM_OVERRIDECONFIG << ")" << std::endl <<
                    "-l or --logfile filename   Writes STDERR and STDOUT messages to filename" << std::endl <<
                    "-v or --verbose            Prints more debug output" << std::endl;
            return EXIT_INVALID_CMDLINE;
        }
    }

    // set up log file
    int logfd = -1;

    if (!logfile.empty())
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
        std::cout << "Unable to ignore SIGPIPE\n";

    //  Switch to daemon mode?
    if (daemon_mode)
    {
        if (daemon(0, 0) < 0)
        {
            std::cout << "Failed to run as a daemon. Bailing out.\n";
            return EXIT_DAEMONIZING_ERROR;
        }
        std::cout << std::endl;
    }

    std::map<int, ZMServer*> serverList; // list of ZMServers

    // load the default config
    loadZMConfig(zmconfig);

    // load the override config
    loadZMConfig(zmoverideconfig);

    // check we have a version (default to 1.34.16 if not found)
    if (g_zmversion.empty())
    {
        std::cout << "ZM version not found. Assuming at least v1.34.16 is installed" << std::endl;
        g_majorVersion = 1;
        g_minorVersion = 34;
        g_revisionVersion = 16;
    }
    else
    {
        sscanf(g_zmversion.c_str(), "%10d.%10d.%10d", &g_majorVersion, &g_minorVersion, &g_revisionVersion);

        // we support version 1.24.0 or later
        if (checkVersion(1, 24, 0))
        {
            std::cout << "ZM is version '" << g_zmversion << "'" << std::endl;
        }
        else
        {
            std::cout << "This version of ZM is to old you need 1.24.0 or later '" << g_zmversion << "'" << std::endl;
            return EXIT_VERSION_ERROR;
        }
    }

    // connect to the DB
    connectToDatabase();

    // clear the master and temp sets
    FD_ZERO(&master);   // NOLINT(readability-isolate-declaration)
    FD_ZERO(&read_fds); // NOLINT(readability-isolate-declaration)

    // get the listener
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1)
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
    if (::bind(listener, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1)
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

    std::cout << "Listening on port: " << port << std::endl;

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    while (!quit)
    {
        // the maximum time select() should wait
        struct timeval timeout {DB_CHECK_TIME.count(), 0};

        read_fds = master; // copy it
        int res = select(fdmax+1, &read_fds, nullptr, nullptr, &timeout);

        if (res == -1)
        {
            perror("select");
            return EXIT_SOCKET_ERROR;
        }
        if (res == 0)
        {
            // select timed out
            // just kick the DB connection to keep it alive
            kickDatabase(debug);
            continue;
        }

        // run through the existing connections looking for data to read
        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                // we got one!!
                if (i == listener) 
                {
                    // handle new connections
                    socklen_t addrlen = sizeof(remoteaddr);
                    newfd = accept(listener, (struct sockaddr *) &remoteaddr,
                                   &addrlen);
                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        // add to master set
                        FD_SET(newfd, &master);
                        fdmax = std::max(newfd, fdmax); // keep track of the maximum

                        // create new ZMServer and add to map
                        auto *server = new ZMServer(newfd, debug);
                        serverList[newfd] = server;

                        printf("new connection from %s on socket %d\n",
                               inet_ntoa(remoteaddr.sin_addr), newfd);
                    }
                }
                else
                {
                    // handle data from a client
                    int nbytes = recv(i, buf.data(), buf.size(), 0);
                    if (nbytes <= 0)
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

                        // remove from master set
                        FD_CLR(i, &master);

                        close(i);

                        // remove from server list
                        ZMServer *server = serverList[i];
                        delete server;
                        serverList.erase(i);
                    }
                    else
                    {
                        ZMServer *server = serverList[i];
                        quit = server->processRequest(buf.data(), nbytes);
                    }
                }
            }
        }
    }

    // cleanly remove all the ZMServer's
    for (auto & server : serverList)
        delete server.second;

    mysql_close(&g_dbConn);

    return EXIT_OK;
}

