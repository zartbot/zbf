#include "io.h"

static inline void
fetch_flowkey(struct rte_mbuf *pkt, struct flow_key *key)
{
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;

    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    key->ip_src = 0;
    key->ip_dst = 0;
    if (likely(eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)))
    {
        ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
        key->ip_src = rte_be_to_cpu_32(ipv4_hdr->src_addr);
        key->ip_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
        rte_pktmbuf_free(pkt);
        return;
    }
    if (likely(eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)))
    {
        struct rte_vlan_hdr *vlan_hdr;
        vlan_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_vlan_hdr *, sizeof(struct rte_ether_hdr));
        //printf("vlan id : %d\n", rte_be_to_cpu_16(vlan_hdr->vlan_tci));
        if (likely(vlan_hdr->eth_proto == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)))
        {
            ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_vlan_hdr));
            key->ip_src = rte_be_to_cpu_32(ipv4_hdr->src_addr);
            key->ip_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
            rte_pktmbuf_free(pkt);
            return;
        }
        rte_pktmbuf_free(pkt);
        return;
    }
}

int lcore_decode(struct decode_lcore_params *p)
{
    printf("Core %u doing packet RX.\n", rte_lcore_id());

    uint64_t freq = rte_get_tsc_hz() * 1;
    uint64_t pkt_cnt = 0;
    uint64_t now = rte_rdtsc();

    while (1)
    {
        struct rte_mbuf *pkts[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(ETH_PORT_ID, p->rx_qid, pkts,
                                                BURST_SIZE);

        /* Check performance */
        if (unlikely(rte_rdtsc() - now > freq))
        {
            printf("RX-Queue[%d] PPS: %10ld ring_depth: %10d\n", p->rx_qid, pkt_cnt,  rte_ring_count(p->telemetry_ring));
            pkt_cnt = 0;
            now = rte_rdtsc();
        }
        pkt_cnt += nb_rx;
        

        if (unlikely(nb_rx == 0))
        {
            continue;
        }

        struct telemetry *t = malloc(sizeof(struct telemetry));
        t->num = nb_rx;
        int i;
        /* Prefetch first packets */
        for (i = 0; i < PREFETCH_OFFSET && i < nb_rx; i++)
        {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
        }
        for (i = 0; i < (nb_rx - PREFETCH_OFFSET); i++)
        {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i + PREFETCH_OFFSET], void *));
            fetch_flowkey(pkts[i], &t->key[i]);
        }

        /* Process left packets */
        for (; i < nb_rx; i++)
        {
            fetch_flowkey(pkts[i], &t->key[i]);
        }

        /* verify flow key
        for (int i = 0; i < t->num; i++){
            printf("%d.%d.%d.%d--->%d.%d.%d.%d\n", NIPQUADR(t->key[i].ip_src), NIPQUADR(t->key[i].ip_dst));
        } */
        rte_ring_sp_enqueue(p->telemetry_ring, (void *)t);
    }
    return 0;
}

int lcore_service(struct service_lcore_params *p)
{
    uint16_t ret_vbf,set_vbf;
    printf("Core %u doing bloom filter checking.\n", rte_lcore_id());
    while (1)
    {
        struct telemetry *t[BURST_SIZE];
        const uint16_t nb_rx = rte_ring_dequeue_burst(p->telemetry_ring,
                                                      (void *)t, BURST_SIZE, NULL);
        if (unlikely(nb_rx == 0))
        {
            continue;
        }
        /* check flow*/
        for (int i = 0; i < nb_rx; i++)
        {
            uint16_t *sets = (uint16_t *)malloc(sizeof(uint16_t) * t[i]->num);
            
            for (int j = 0; j < t[i]->num; j++)
            {
                ret_vbf = rte_member_lookup(p->setsum, &t[i]->key[j], &set_vbf);
                if (unlikely(set_vbf == 0 || ret_vbf == 0))
                {
                    rte_member_add(p->setsum, t[i]->key, p->rx_qid + 1);

                    struct flow_info *f = malloc(sizeof(struct flow_info));
                    f->ip_src = rte_cpu_to_be_32(t[i]->key[j].ip_src);
                    f->ip_dst = rte_cpu_to_be_32(t[i]->key[j].ip_dst);
                    rte_ring_enqueue(p->export_ring,(void *)f);
                    //printf("%d.%d.%d.%d--->%d.%d.%d.%d\n", NIPQUADR(t[i]->key[j].ip_src), NIPQUADR(t[i]->key[j].ip_dst));
                }
            }
            free(t[i]);
        }
    }
    return 0;
}
