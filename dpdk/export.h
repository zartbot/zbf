#include "common.h"



struct export_lcore_params
{
    struct rte_ring *export_ring;
    uint64_t export_rate;
    int dport;
    char ip_addr[255];
};

int lcore_export(struct export_lcore_params *p);


