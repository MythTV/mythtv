/*
 *  firewire_tester
 *  Copyright (c) 2006 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 *
 *  $ gcc -Wall -o firewire_tester firewire_tester.c -liec61883 -lraw1394
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/select.h>
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#define ACTION_NONE        -1
#define ACTION_TEST_BCAST   0
#define ACTION_TEST_P2P     1
#define ACTION_FIX_BCAST    2
#define ACTION_RESET_BUS    3
#define ACTION_OUTPUT_P2P   4

#define SYNC_BYTE           0x47
#define MIN_PACKETS         25
#define MAX_NODATA          10

#define VERBOSE(args...)    do { if (verbose) printf(args); } while (0)

int verbose = 0;
int sync_failed = 0;
int nodata = 0;
int run = 0;

static int read_packet (unsigned char *tspacket, int len, 
                        unsigned int dropped, void *callback_data)
{
    int *count = (int *)callback_data;

    if (dropped)
    {
        printf("Dropped %d packet(s).\n", dropped);
        return 0;
    }

    if (tspacket[0] != SYNC_BYTE)
    {
        sync_failed = 1;
        return 0;
    }
    nodata = 0;
    *count = *count + 1;
    return 1;
}

static int fwrite_packet (unsigned char *tspacket, int len, 
                        unsigned int dropped, void *callback_data)
{
    int n;
    FILE *fp = (FILE *) callback_data;

    if (dropped)
    {
        fprintf(stderr, "Dropped %d packet(s).\n", dropped);
        return 0;
    }

    if (tspacket[0] != SYNC_BYTE)
    {
        sync_failed = 1;
        return 0;
    }
    nodata = 0;
    n = fwrite(tspacket, sizeof(char), 188, fp);
    //fprintf(stderr, "fwrite_packets: wrote %d of %d\n", n, len);
    return(n == len ? 0 : 1);
}

int test_connection(raw1394handle_t handle, int channel)
{
    int count = 0;
    int retry = 0;
    int fd = raw1394_get_fd(handle);
    iec61883_mpeg2_t mpeg;
    struct timeval tv;
    fd_set rfds;

    sync_failed = 0;
    mpeg = iec61883_mpeg2_recv_init(handle, read_packet, (void*) &count);
    iec61883_mpeg2_recv_start(mpeg, channel);
    while(count < MIN_PACKETS && retry < 2 && !sync_failed 
          && nodata < MAX_NODATA)
    {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0)
        {
             nodata++;
             raw1394_loop_iterate(handle);
        }
        else
        {
            retry++;
        }
    }
    iec61883_mpeg2_recv_stop(mpeg);
    iec61883_mpeg2_close(mpeg);

    if (sync_failed)
        return 0;

    return count;
}

// create and test a p2p connection
// returns 1 on success, 0 on failure
int test_p2p(raw1394handle_t handle, nodeid_t node) {
    int oplug = -1, iplug = -1, bandwidth = -1;
    int channel, count, success = 0;

    printf("P2P: Testing...");
    fflush(stdout);

    // make connection
    channel = iec61883_cmp_connect(handle, node, &oplug,
		raw1394_get_local_id(handle), &iplug, &bandwidth);

    VERBOSE("P2P: Creating, node %d, channel %d\n", node, channel);
    fflush(stdout);

#if 0
    if (iec61883_cmp_create_p2p_output(handle, node | 0xffc0, 0, channel,
                                       1 /* fix me, speed */ ) != 0)
    {
        printf("iec61883_cmp_create_p2p_output failed\n");
        return 0;
    }
#endif

    count = test_connection(handle, channel);
    if (count >= MIN_PACKETS)
    {
        printf("Success, %d packets received\n", count);
        success = 1;
    }
    else
    {
        printf("Failed%s\n", (sync_failed ? " (sync failed)":""));
    }

    VERBOSE("P2P: Disconnecting.\n");
    iec61883_cmp_disconnect(handle, node | 0xffc0, 0,
                            raw1394_get_local_id (handle),
                            -1, channel, 0);
    return success;
}

// create and test a broadcast connection
// returns 1 on success, 0 on failure
int test_broadcast(raw1394handle_t handle, nodeid_t node) {
    int channel, count, success = 0;
    channel = 63 - node;

    VERBOSE("Broadcast: Creating, node %d, channel %d\n", node, channel);

    printf("Broadcast: Testing...");
    fflush(stdout);

    // open connection
    if (iec61883_cmp_create_bcast_output(handle, node | 0xffc0, 0, channel, 
                                         1 /* fix me, speed */ ) != 0)
    {
        printf("iec61883_cmp_create_bcast_output failed\n");
        return 0;
    }
    count = test_connection(handle, channel);

    if (count >= MIN_PACKETS)
    {
        printf("Success, %d packets\n", count);
        success = 1;
    }
    else
    {
        printf("Failed%s\n", (sync_failed ? " (sync failed)":""));
    }

    VERBOSE("Broadcast: Disconnecting.\n");
    iec61883_cmp_disconnect(handle, node | 0xffc0, 0,
                            raw1394_get_local_id (handle),
                            -1, channel, 0);
    return success;
}  

/* 
 *  Attempt to get a reliable broadcast connection initialized
 *  This is done by first attempting multiple p2p connections until data is 
 *  received, once data is seen we then attempt multiple (5) broadcast 
 *  connections to verify the connection is stable.
 *  returns 1 on success, 0 on fail.
 */
int fix_broadcast(raw1394handle_t handle, nodeid_t node) {
    int p2p_retries = 0;
    int bcast_success = 0;

    // see if we even need to fix it
    while (test_broadcast(handle, node))
    {
        bcast_success++;
        if (bcast_success == 5)
        {
            printf("Broadcast Fix: Success (already stable)\n");
            return 1;
        }
    }

    // attempt upto 10 p2p connections looking for data
    while (p2p_retries < 10)
    { 
        if (test_p2p(handle, node))
        {
            // got data from p2p, try a few bcast connections
            bcast_success = 0;
            while (test_broadcast(handle, node))
            {
                bcast_success++;
                if (bcast_success == 5)
                {
                    printf("Broadcast Fix: Success\n");
                    return 1;
                }
            }
        }
        p2p_retries++;
    }
    printf("Broadcast Fix: Failed\n");
    return 0;
}

// create and test a p2p connection, copy packets to stdout
// returns 1 on success, 0 on failure
int output_p2p(raw1394handle_t handle, nodeid_t node) {
    int channel, count = 0, success = 0;
    int oplug = -1, iplug = -1, bandwidth = -1;
    int retry = 0;
    FILE *fp = stdout;
    iec61883_mpeg2_t mpeg;
    struct timeval tv;
    fd_set rfds;
    int fd = raw1394_get_fd(handle);

    sync_failed = 0;

    // make connection
    channel = iec61883_cmp_connect(handle, node, &oplug,
		raw1394_get_local_id(handle), &iplug, &bandwidth);
#if 0
    if (iec61883_cmp_create_p2p_output(handle, node | 0xffc0, 0, channel,
                                       1 /* fix me, speed */ ) != 0)
    {
        fprintf(stderr, "iec61883_cmp_create_p2p_output failed\n");
        return 0;
    }
#endif

    VERBOSE("P2P: Creating, node %d, channel %d\n", node, channel);
    fflush(stdout);

    mpeg = iec61883_mpeg2_recv_init(handle, fwrite_packet, (void*) fp);
    run = 1;
    iec61883_mpeg2_recv_start(mpeg, channel);
    while(run && retry < 2 && !sync_failed && nodata < MAX_NODATA)
    {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0)
        {
             nodata++;
             raw1394_loop_iterate(handle);
        }
        else
        {
            retry++;
        }
    }
    iec61883_mpeg2_recv_stop(mpeg);
    iec61883_mpeg2_close(mpeg);

    if (sync_failed)
        return 0;

    return count;

    VERBOSE("P2P: Disconnecting.\n");
    iec61883_cmp_disconnect(handle, node | 0xffc0, 0,
                            raw1394_get_local_id (handle),
                            -1, channel, 0);
    return success;
}

void usage(void) {
    printf("firewire_tester <action> -n <node> [-P <port>] [-r <n>] [-v]\n");
    printf(" Actions: (one is required)\n");
    printf("    -b          - test broadcast connection\n");
    printf("    -p          - test p2p connection\n");
    printf("    -B          - attempt to fix/stabilize broadcast connection\n");
    printf("    -R          - reset the firewire bus\n");
    printf(" Options\n");
    printf("    -n <node>   - firewire node, required\n");
    printf("    -o          - write packets to stdout using p2p connection\n");
    printf("    -P <port>   - firewire port, default 0\n");
    printf("    -r <n>      - run action <n> times, default 1\n");
    printf("    -v          - verbose\n");
    exit(1);
}

int main(int argc, char **argv) {
    raw1394handle_t handle;
    int node = -1;
    int port = 0;
    int runs = 1;
    int c, i, success;
    int action = ACTION_NONE;

    opterr = 0;
    while ((c = getopt(argc, argv, "Bbn:opP:r:Rv")) != -1)
    {
        switch (c) 
        {

            // attempt to get a reliable bcast connection initialize
            case 'B':
                if (action != ACTION_NONE)
                {
                    printf("Invalid command line\n");
                    usage();
                }
                action = ACTION_FIX_BCAST;
                break;

            // test broadcast connection
            case 'b':
                if (action != ACTION_NONE)
                {
                    printf("Invalid command line\n");
                    usage();
                }
                action = ACTION_TEST_BCAST;
                break;

            // set the node, required
            case 'n':
                node = atoi(optarg);
                if (node < 0 || node > 63)
                {
                    printf("Invalid node: %d\n", node);
                    exit(1);
                }
                break;

            // output packets using p2p connection
            case 'o':
                if (action != ACTION_NONE)
                {
                    printf("Invalid command line\n");
                    usage();
                }
                action = ACTION_OUTPUT_P2P;
                break;

            // set the port, optional
            case 'P':
                port = atoi(optarg);
                if (port < 0)
                {
                    printf("Invalid port: %d\n", port);
                }
                break;

            // test a p2p connection
            case 'p':
                if (action != ACTION_NONE)
                {
                    printf("Invalid command line\n");
                    usage();
                }
                action = ACTION_TEST_P2P;
                break;

            case 'R':
                action = ACTION_RESET_BUS;
                break;

            // number of runs
            case 'r':
                runs = atoi(optarg);
                if (runs <= 0)
                {
                    printf("Run amount <= 0\n");
                    usage();
                }
                break;

            // verbose
            case 'v':
                verbose = 1;
                break;

            // bad option
            default:
                printf("Invalid command line\n");
                usage();
                
        }
    }

    if (action == ACTION_NONE)
    {
        usage();
    }

    if (action != ACTION_RESET_BUS && node == -1)
    {
        printf("-n <node> is a required option\n");
        usage();
    }

    VERBOSE("raw1394: Allocating handle, port %d.\n", port);
    handle = raw1394_new_handle_on_port(port);
    if (!handle)
    {
        printf("Failed to create new raw1394 handle on port %d\n", port);
        exit(1);
    }

    success = 0;
    switch (action)
    {
        case ACTION_TEST_BCAST:
            printf("Action: Test broadcast %d times, node %d, channel %d\n", 
                   runs, node, 63 - node);
            for (i = 0;i < runs;i++) 
                success += test_broadcast(handle, node);
            break;
        case ACTION_TEST_P2P:
            printf("Action: Test P2P connection %d times, node %d, channel %d\n",
                   runs, node, node);
            for (i = 0;i < runs;i++)
                success += test_p2p(handle, node);
            break;
        case ACTION_FIX_BCAST:
            printf("Action: Attempt to fix broadcast connection %d times, node %d\n", 
                   runs, node);
            for (i = 0;i < runs;i++)
                success += fix_broadcast(handle, node);
            break;
        case ACTION_RESET_BUS:
            runs = 1;
            printf("Action: Resetting the firewire bus\n");
            if (raw1394_reset_bus_new(handle, RAW1394_LONG_RESET) == 0)
            {
                printf("Bus reset succeeded\n");
                success = 1;
            }
            else
            {
                printf("Bus reset failed: %d\n", errno);
            }
            break;
        case ACTION_OUTPUT_P2P:
            runs = 1;
            fprintf(stderr,
                    "Action: Output P2P packets using node %d, channel %d\n",
                    node, node);
            success = output_p2p(handle, node);
            break;
    }

    VERBOSE("raw1394: Releasing handle.\n");
    raw1394_destroy_handle(handle);
    exit(!(success == runs));
}
