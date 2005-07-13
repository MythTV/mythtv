#ifndef _1035_h
#define _1035_h

// be familiar with rfc1035 if you want to know what all the variable names mean, but this hides most of the dirty work
// all of this code depends on the buffer space a packet is in being 4096 and zero'd before the packet is copied in
// also conveniently decodes srv rr's, type 33, see rfc2782

// should be reasonably large, for udp
#define MAX_PACKET_LEN 4000

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

struct question
{
    unsigned char *name;
    uint16_t type, a_class;
};

#define QTYPE_A 1
#define QTYPE_NS 2
#define QTYPE_CNAME 5
#define QTYPE_PTR 12
#define QTYPE_SRV 33

struct resource
{
    unsigned char *name;
    uint16_t type, a_class;
    uint32_t ttl;
    uint16_t rdlength;
    unsigned char *rdata;
    union {
        struct { uint32_t ip; char *name; } a;
        struct { unsigned char *name; } ns;
        struct { unsigned char *name; } cname;
        struct { unsigned char *name; } ptr;
        struct { uint16_t priority, weight, port; unsigned char *name; } srv;
    } known;
};

struct message
{
    // external data
    uint16_t id;
    struct { unsigned short qr:1, opcode:4, aa:1, tc:1, rd:1, ra:1, z:3, rcode:4; } header;
    uint16_t qdcount, ancount, nscount, arcount;
    struct question *qd;
    struct resource *an, *ns, *ar;

    // internal variables
    unsigned char *_buf, *_labels[20];
    int _len, _label;
    
    // packet acts as padding, easier mem management
    unsigned char _packet[MAX_PACKET_LEN];
};

// returns the next short/long off the buffer (and advances it)
uint16_t net2short(unsigned char **buf);
uint32_t net2long(unsigned char **buf);

// copies the short/long into the buffer (and advances it)
void short2net(uint16_t i, unsigned char **buf);
void long2net(uint32_t l, unsigned char **buf);

// parse packet into message, packet must be at least MAX_PACKET_LEN and message must be zero'd for safety
void message_parse(struct message *m, unsigned char *packet);

// create a message for sending out on the wire
struct message *message_wire(void);

// append a question to the wire message
void message_qd(struct message *m, unsigned char *name, uint16_t type, uint16_t a_class);

// append a resource record to the message, all called in order!
void message_an(struct message *m, unsigned char *name, uint16_t type, uint16_t a_class, uint32_t ttl);
void message_ns(struct message *m, unsigned char *name, uint16_t type, uint16_t a_class, uint32_t ttl);
void message_ar(struct message *m, unsigned char *name, uint16_t type, uint16_t a_class, uint32_t ttl);

// append various special types of resource data blocks
void message_rdata_long(struct message *m, uint32_t l);
void message_rdata_name(struct message *m, unsigned char *name);
void message_rdata_srv(struct message *m, uint16_t priority, uint16_t weight, uint16_t port, unsigned char *name);
void message_rdata_raw(struct message *m, unsigned char *rdata, uint16_t rdlength);

// return the wire format (and length) of the message, just free message when done
unsigned char *message_packet(struct message *m);
int message_packet_len(struct message *m);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
