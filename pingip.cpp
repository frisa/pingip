#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define PACKETSIZE 64
#define SOL_IP 0

struct icmphdr
{
    u_int8_t type; /* message type */
    u_int8_t code; /* type sub-code */
    u_int16_t checksum;
    union
    {
        struct
        {
            u_int16_t id;
            u_int16_t sequence;
        } echo;            /* echo datagram */
        u_int32_t gateway; /* gateway address */
        struct
        {
            u_int16_t unused;
            u_int16_t mtu;
        } frag; /* path mtu discovery */
    } un;
};

struct packet
{
    struct icmphdr hdr;
    char msg[PACKETSIZE - sizeof(struct icmphdr)];
};

int pid = -1;
struct protoent *proto = NULL;
int cnt = 1;

unsigned short checksum(void *b, int len)
{
    unsigned short *buf = (unsigned short *)b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int ping(char *adress)
{
    const int val = 255;
    int i, sd;
    struct packet pckt;
    struct sockaddr_in r_addr;
    int loop;
    struct hostent *hname;
    struct sockaddr_in addr_ping, *addr;

    pid = getpid();
    proto = getprotobyname("ICMP");
    hname = gethostbyname(adress);
    bzero(&addr_ping, sizeof(addr_ping));
    addr_ping.sin_family = hname->h_addrtype;
    addr_ping.sin_port = 0;
    addr_ping.sin_addr.s_addr = *(long *)hname->h_addr;

    addr = &addr_ping;

    sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
    if (sd < 0)
    {
        perror("socket");
        return 1;
    }
    if (setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
    {
        perror("Set TTL option");
        return 1;
    }
    if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
    {
        perror("Request nonblocking I/O");
        return 1;
    }

    for (loop = 0; loop < 10; loop++)
    {

        socklen_t len = sizeof(r_addr);

        if (recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr *)&r_addr, &len) > 0)
        {
            return 0;
        }

        bzero(&pckt, sizeof(pckt));
        pckt.hdr.type = ICMP_ECHO;
        pckt.hdr.un.echo.id = pid;
        for (i = 0; i < sizeof(pckt.msg) - 1; i++)
            pckt.msg[i] = i + '0';
        pckt.msg[i] = 0;
        pckt.hdr.un.echo.sequence = cnt++;
        pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));
        if (sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr *)addr, sizeof(*addr)) <= 0)
            perror("sendto");

        usleep(300000);
    }

    return 1;
}

int main()
{
    if (ping("www.google.com"))
        printf("Ping is not OK. \n");
    else
        printf("Ping is OK. \n");
    return 0;
}