#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#include "portinit.h"
#include "io.h"
#include "export.h"
#include "cli_parser.h"

int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;

    config.first_lcore = 24;
    config.num_service_core = 4;
    sprintf(config.ip_addr ,"%s","192.168.99.101");
    config.dport = 12345;
    config.export_rate = 50;

    struct rte_ring *export_ring;
    struct decode_lcore_params lpd[MAX_SERVICE_CORE];
    struct service_lcore_params lps[MAX_SERVICE_CORE];
    struct export_lcore_params exp;

    int retval = rte_eal_init(argc, argv);
    if (retval < 0)
        rte_exit(EXIT_FAILURE, "initialize fail!");

    retval = zspan_args_parser(argc, argv, &config);
    if (retval < 0)
        rte_exit(EXIT_FAILURE, "Invalid arguments\n");

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * 4,
                                        MBUF_CACHE_SIZE, 8,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    struct rte_ring *data_ring[MAX_SERVICE_CORE];

    struct rte_member_setsum *setsum_vbf;

    static struct rte_member_parameters params = {
        .num_keys = MAX_ENTRIES, /* Total hash table entries. */
        .key_len = 8,            /* Length of hash key. */

        /* num_set and false_positive_rate only relevant to vBF */
        .num_set = 32,
        .false_positive_rate = 0.001,
        .prim_hash_seed = 1,
        .sec_hash_seed = 11,
        .socket_id = 1 /* NUMA Socket ID for memory. */
    };
    params.name = "vbf_name";
    params.type = RTE_MEMBER_TYPE_VBF;
    //params.type = RTE_MEMBER_TYPE_HT;
    setsum_vbf = rte_member_create(&params);
    if (setsum_vbf == NULL)
    {
        printf("Creation of setsum_vbf fail\n");
        return -1;
    }

    /* create rings and lparams*/
    export_ring = rte_ring_create("export_ring", 1024 * 1024 * 16,
                                  rte_socket_id(), RING_F_SC_DEQ | RING_F_MP_HTS_ENQ);
    if (export_ring == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create export ring\n");

    exp.export_ring = export_ring;
    exp.export_rate = config.export_rate;       
    exp.dport = config.dport;
    strncpy(exp.ip_addr,config.ip_addr,255);
    
    for (int i = 0; i < config.num_service_core; ++i)
    {
        char rx_ring_name[14];
        sprintf(rx_ring_name, "input_ring%d", i);
        data_ring[i] = rte_ring_create(rx_ring_name, 1024 * 1024 * 16,
                                       rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
        if (data_ring[i] == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create input ring\n");
        lpd[i].telemetry_ring = data_ring[i];
        lps[i].telemetry_ring = data_ring[i];
        lps[i].export_ring = export_ring;
        lpd[i].rx_qid = i;
        lps[i].rx_qid = i;
        lps[i].setsum = setsum_vbf;
    }

    /* Initialize eth port */
    if (port_init(ETH_PORT_ID, mbuf_pool, config.num_service_core, 1) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", ETH_PORT_ID);

    printf("system init finished, starting service process...\n");


    unsigned int lcore_num = config.first_lcore;
    rte_eal_remote_launch((lcore_function_t *)lcore_export, &exp, lcore_num++);
   
    /* Start IO-RX process */
    for (int i = 0; i < config.num_service_core; ++i)
    {
        rte_eal_remote_launch((lcore_function_t *)lcore_decode, &lpd[i], lcore_num++);
        rte_eal_remote_launch((lcore_function_t *)lcore_service, &lps[i], lcore_num++);
    }

    rte_eal_wait_lcore(config.first_lcore);
    return 0;
}
