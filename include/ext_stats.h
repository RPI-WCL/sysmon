#ifndef _EXT_STATS_H
#define _EXT_STATS_H

#include "common.h"

/*
 ***************************************************************************
 * Definitions of structures for extended system statistics
 ***************************************************************************
 */
struct ext_stats_cpu {
    double utils[CPU_STATES];   /* % */
}; 

struct ext_stats_disk {
    double tps;                 /* Transactions / sec */
    double rd_kbps, wr_kbps;    /* KBytes / sec */
    double util;                /* % */
}; 

struct ext_stats_net {
    double rx_pktps, tx_pktps;  /* Packets / sec */
    double rx_kbps, tx_kbps;    /* KBytes / sec */
    double util;                /* % */
};

struct ext_stats_mem {
    unsigned long mem_used;     /* KBytes */
    double util;                /* % */
};

void compute_ext_cpu_stats(struct stats_cpu *cpu_curr,         /* IN */
                           struct stats_cpu *cpu_prev,         /* IN */
                           struct ext_stats_cpu *ext_cpu);     /* OUT */

void compute_ext_sysfs_disk_stats(struct stats_sysfs_disk *disk_curr,        /* IN */
                                  struct stats_sysfs_disk *disk_prev,        /* IN */
                                  unsigned long long interval, int factor,   /* IN */
                                  struct ext_stats_disk *ext_disk);          /* OUT */

void compute_ext_net_stats(struct stats_net_dev *net_curr,      /* IN */
                           struct stats_net_dev *net_prev,      /* IN */
                           unsigned long long interval,         /* IN */
                           struct ext_stats_net *ext_net);      /* OUT */

void compute_ext_mem_stats(struct stats_memory *mem,            /* IN */
                           struct ext_stats_mem *ext_mem);      /* OUT */


#endif /* #ifndef _EXT_STATS_H */
