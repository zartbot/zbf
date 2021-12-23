#include "export.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BATCH_SIZE 176


int lcore_export(struct export_lcore_params *p){
    printf("Core %u doing exporting.\n", rte_lcore_id());

    uint64_t freq = rte_get_tsc_hz() * 1;
    uint64_t token = p->export_rate;
    uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;

    const uint64_t drain_tsc = (rte_get_tsc_hz() + MS_PER_S - 1) / MS_PER_S;
    prev_tsc = 0;
    timer_tsc = 0;


    int sockfd;
    char msg[10000];
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    //Punt udp socket
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(p->dport);
    servaddr.sin_addr.s_addr = inet_addr(p->ip_addr); 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    while (1)
    {
        cur_tsc = rte_rdtsc();
        diff_tsc = cur_tsc - prev_tsc;
        if (unlikely(diff_tsc > drain_tsc))
        {
            //reset token
            prev_tsc = cur_tsc;
            token = p->export_rate;
        }

        if (unlikely(token <= 0))
        {
            continue;
        }
        struct flow_info *f[BATCH_SIZE];
        const uint16_t nb_rx = rte_ring_dequeue_burst(p->export_ring,
                                                      (void *)f, BATCH_SIZE, NULL);
        if (unlikely(nb_rx == 0))
        {
            continue;
        }
        /* check flow*/
        
        for (int i = 0; i < nb_rx; i++)
        {
            memcpy(&msg[i<<3],f[i],sizeof(struct flow_info));
            free(f[i]);
        }
        sendto(sockfd, msg, nb_rx <<3,
                   MSG_CONFIRM, (const struct sockaddr *)&servaddr,
                   sizeof(servaddr));
        token--;                   
    }
    return 0;
}
