#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PING_PKT_S 64
#define RECV_TIMEOUT 1

using namespace std;

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
            u_int16_t neco;
            u_int16_t mtu;
        } frag; /* path mtu discovery */
    } un;
};
struct iphdr
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ihl : 4;
    unsigned int version : 4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int version : 4;
    unsigned int ihl : 4;
#else
#error "Please fix <bits/endian.h>"
#endif
    u_int8_t tos;
    u_int16_t tot_len;
    u_int16_t id;
    u_int16_t frag_off;
    u_int8_t ttl;
    u_int8_t protocol;
    u_int16_t check;
    u_int32_t saddr;
    u_int32_t daddr;
    /*The options start here. */
};

struct iphdr *ip;
struct icmphdr *icmp;

void setupPacket(int seq_no, char *packet)
{
    icmp = (struct icmphdr *)(packet + sizeof(struct iphdr));
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.sequence = seq_no;
    icmp->un.echo.id = getpid();
    icmp->checksum = 0;
    icmp->checksum = 0;
}

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

int main(int argc, char *argv[])
{
    const char* target_ip;
    if (argc != 2)
    {
        cout << "Usage: " << argv[0] << " <destination_IP>" << endl;
        std::string default_host = "www.google.com";
        cout << "default host: " << default_host;
        target_ip = default_host.c_str();
    }
    else
    {
        target_ip = argv[1];
    }

    int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (raw_sock < 0)
    {
        std::cout << "error: " << errno;
        perror("Socket Error");
        return 1;
    }

    char packet[PING_PKT_S];
    setupPacket(0, packet);

    struct sockaddr_in dest;
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = 0;
    dest.sin_addr.s_addr = inet_addr(target_ip);

    cout << "PING " << target_ip << " (" << target_ip << "): " << PING_PKT_S << " data bytes" << endl;

    int seq_no = 0;
    while (true)
    {
        icmp->un.echo.sequence = seq_no++;
        icmp->checksum = 0;
        icmp->checksum = checksum((void *)icmp, PING_PKT_S);
        int send_result = sendto(raw_sock, packet, PING_PKT_S, 0, (struct sockaddr *)&dest, sizeof(dest));
        if (send_result == -1)
        {
            perror("Packet Send Error");
        }

        char recv_buffer[PING_PKT_S];
        struct sockaddr_in recv_addr;
        socklen_t addr_len = sizeof(recv_addr);
        struct timeval timeout;
        timeout.tv_sec = RECV_TIMEOUT;
        timeout.tv_usec = 0;

        if (setsockopt(raw_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
        {
            perror("Set Socket Timeout Error");
            return 1;
        }

        ssize_t recv_result = recvfrom(raw_sock, recv_buffer, PING_PKT_S, 0, (struct sockaddr *)&recv_addr, &addr_len);
        if (recv_result < 0)
        {
            cout << "Request timed out." << endl;
        }
        else
        {
            cout << PING_PKT_S << " bytes from " << target_ip << ": icmp_seq=" << seq_no << " ttl=64 time=" << recv_result << " ms" << endl;
        }

        sleep(1); // Wait for 1 second before sending the next packet.
    }

    close(raw_sock);
    return 0;
}
