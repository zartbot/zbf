#include "common.h"

/* 2-tuple key type */
struct flow_key
{
    uint32_t ip_src;
    uint32_t ip_dst;
} __rte_packed;

struct telemetry {
    uint16_t num;
    struct flow_key key[32];
};


struct decode_lcore_params
{
    struct rte_ring *telemetry_ring;
    struct rte_mempool *mem_pool;
    uint16_t rx_qid;
};

struct service_lcore_params
{
    struct rte_ring *telemetry_ring;
    struct rte_ring *export_ring;
    struct rte_member_setsum *setsum;
    uint16_t rx_qid;
};


int lcore_decode(struct decode_lcore_params *p);

int lcore_service(struct service_lcore_params *p);


