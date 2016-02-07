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
    double rd_kB, wr_kB;        /* KBytes */
    double rd_kBps, wr_kBps;    /* KBytes / sec */
    double util;                /* % */
}; 

struct ext_stats_net {
    double rx_pktps, tx_pktps;  /* Packets / sec */
    double rx_kBps, tx_kBps;    /* KBytes / sec */
    double util;                /* % */
};

struct ext_stats_mem {
    union {
        unsigned long long mem_used;    /* KBytes, min/max */
        double mem_used_;               /* mean/var */
    } u;
    double util;                        /* % */
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

void update_ext_cpu_stats(int n,                        /* IN */
                          struct ext_stats_cpu *cpu,    /* IN */
                          struct ext_stats_cpu *min,    /* OUT */
                          struct ext_stats_cpu *max,    /* OUT */
                          struct ext_stats_cpu *mean,   /* OUT */
                          struct ext_stats_cpu *var);   /* OUT */

void update_ext_disk_stats(int n,                        /* IN */
                           struct ext_stats_disk *disk, /* IN */
                           struct ext_stats_disk *min,  /* OUT */
                           struct ext_stats_disk *max,  /* OUT */
                           struct ext_stats_disk *mean, /* OUT */
                           struct ext_stats_disk *var); /* OUT */

void update_ext_net_stats(int n,                        /* IN */
                          struct ext_stats_net *net,    /* IN */
                          struct ext_stats_net *min,    /* OUT */
                          struct ext_stats_net *max,    /* OUT */
                          struct ext_stats_net *mean,   /* OUT */
                          struct ext_stats_net *var);   /* OUT */

void update_ext_mem_stats(int n,                        /* IN */
                          struct ext_stats_mem *mem,    /* IN */
                          struct ext_stats_mem *min,    /* OUT */
                          struct ext_stats_mem *max,    /* OUT */
                          struct ext_stats_mem *mean,   /* OUT */
                          struct ext_stats_mem *var);   /* OUT */

#endif /* #ifndef _EXT_STATS_H */
