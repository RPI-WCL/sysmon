#include "utils.h"
#include "ext_stats.h"


void compute_ext_cpu_stats(struct stats_cpu *cpu_curr,         /* IN */
                           struct stats_cpu *cpu_prev,         /* IN */
                           struct ext_stats_cpu *ext_cpu)      /* OUT */
{
    int cpu_percent[CPU_STATES];
    long cpu_diff[CPU_STATES];
    long cpu_time[CPU_STATES] = {cpu_curr->cpu_user, cpu_curr->cpu_sys, cpu_curr->cpu_nice, 
                                 cpu_curr->cpu_idle, cpu_curr->cpu_iowait};
    long cpu_old[CPU_STATES] = {cpu_prev->cpu_user, cpu_prev->cpu_sys, cpu_prev->cpu_nice, 
                                cpu_prev->cpu_idle, cpu_prev->cpu_iowait};
    int i;

    /* convert cpu_time counts to percentages */
    percentages(CPU_STATES, cpu_percent, cpu_time, cpu_old, cpu_diff);
    for (i = 0; i < CPU_STATES; i++)
        ext_cpu->utils[i] = (double)cpu_percent[i] / 10;
}


void compute_ext_sysfs_disk_stats(struct stats_sysfs_disk *disk_curr,        /* IN */
                                  struct stats_sysfs_disk *disk_prev,        /* IN */
                                  unsigned long long interval, int factor,   /* IN */
                                  struct ext_stats_disk *ext_disk)           /* OUT */
{
	unsigned long long rd_sec, wr_sec;

    /* Source: "write_basic_stat" in iostat.c (sysstat) */
	rd_sec = disk_curr->rd_sectors - disk_prev->rd_sectors;
	if ((disk_curr->rd_sectors < disk_prev->rd_sectors) && (disk_prev->rd_sectors <= 0xffffffff)) {
		rd_sec &= 0xffffffff;
	}
	wr_sec = disk_curr->wr_sectors - disk_prev->wr_sectors;
	if ((disk_curr->wr_sectors < disk_prev->wr_sectors) && (disk_prev->wr_sectors <= 0xffffffff)) {
		wr_sec &= 0xffffffff;
    }
    ext_disk->tps     = S_VALUE(disk_prev->rd_ios + disk_prev->wr_ios, \
                                disk_curr->rd_ios + disk_curr->wr_ios, interval);
    ext_disk->rd_kB   = (unsigned long long) rd_sec / factor;
	ext_disk->wr_kB   = (unsigned long long) wr_sec / factor;
    ext_disk->rd_kBps = S_VALUE(disk_prev->rd_sectors, disk_curr->rd_sectors, interval) / factor;
    ext_disk->wr_kBps = S_VALUE(disk_prev->wr_sectors, disk_curr->wr_sectors, interval) / factor;

    /* Source: "compute_ext_disk_stats" in common.c (sysstat) */
	ext_disk->util = S_VALUE(disk_prev->tot_ticks, disk_curr->tot_ticks, interval) / 10.0;
}


void compute_ext_net_stats(struct stats_net_dev *net_curr,      /* IN */
                           struct stats_net_dev *net_prev,      /* IN */
                           unsigned long long interval,         /* IN */
                           struct ext_stats_net *ext_net)       /* OUT */
{
	double rx_kb, tx_kb;

    /* Source: "print_net_dev_stats" in pr_stat.c (sysstat) */
    ext_net->rx_pktps = S_VALUE(net_prev->rx_packets, net_curr->rx_packets, interval);
    ext_net->tx_pktps = S_VALUE(net_prev->tx_packets, net_curr->tx_packets, interval);

    rx_kb = S_VALUE(net_prev->rx_bytes, net_curr->rx_bytes, interval);
    tx_kb = S_VALUE(net_prev->tx_bytes, net_curr->tx_bytes, interval);

    ext_net->rx_kBps = rx_kb / 1024;
    ext_net->tx_kBps = tx_kb/ 1024;
    ext_net->util = compute_ifutil(net_curr, rx_kb, tx_kb);
}


void compute_ext_mem_stats(struct stats_memory *mem,            /* IN */
                           struct ext_stats_mem *ext_mem)       /* OUT */
{
    /* http://serverfault.com/questions/85470/meaning-of-the-buffers-cache-line-in-the-output-of-free */
    ext_mem->u.mem_used = mem->tlmkb - (mem->frmkb + mem->bufkb + mem->camkb);
    ext_mem->util = (double)(ext_mem->u.mem_used * 100 / mem->tlmkb);
}

/* Online mean/variance computation:
   https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm */
static
double compute_mean(int n, double mean, double x)
{
    return (mean + ((x - mean) / n));
}

static
double compute_var(int n, double var, double mean, double mean_prev, double x)
{
    return (((n - 1) * var + (x - mean_prev)*(x - mean)) / n);
}


void update_ext_cpu_stats(int n,                        /* IN */
                          struct ext_stats_cpu *cpu,    /* IN */
                          struct ext_stats_cpu *min,    /* OUT */
                          struct ext_stats_cpu *max,    /* OUT */
                          struct ext_stats_cpu *mean,   /* OUT */
                          struct ext_stats_cpu *var)    /* OUT */
{
    int i;

    for (i = 0; i < CPU_STATES; i++) {
        /* min */
        if (cpu->utils[i] < min->utils[i])      
            min->utils[i] = cpu->utils[i];
        /* max */
        if (max->utils[i] < cpu->utils[i])      
            max->utils[i] = cpu->utils[i];
        /* mean */
        double mean_prev = mean->utils[i];
        mean->utils[i] = compute_mean(n, mean->utils[i], cpu->utils[i]);
        /* var */
        var->utils[i] = compute_var(n, var->utils[i], mean->utils[i], mean_prev, cpu->utils[i]);
    }
}


void update_ext_disk_stats(int n,                       /* IN */
                           struct ext_stats_disk *disk, /* IN */
                           struct ext_stats_disk *min,  /* OUT */
                           struct ext_stats_disk *max,  /* OUT */
                           struct ext_stats_disk *mean, /* OUT */
                           struct ext_stats_disk *var)  /* OUT */
{
    /* min */
    if (disk->tps < min->tps)           min->tps = disk->tps;
    if (disk->rd_kB < min->rd_kB)       min->rd_kB = disk->rd_kB;
    if (disk->wr_kB < min->wr_kB)       min->wr_kB = disk->wr_kB;
    if (disk->rd_kBps < min->rd_kBps)   min->rd_kBps = disk->rd_kBps;
    if (disk->wr_kBps < min->wr_kBps)   min->wr_kBps = disk->wr_kBps;
    if (disk->util < min->util)         min->util = disk->util;
    /* max */
    if (max->tps < disk->tps)           max->tps = disk->tps;
    if (max->rd_kB < disk->rd_kB)       max->rd_kB = disk->rd_kB;
    if (max->wr_kB < disk->wr_kB)       max->wr_kB = disk->wr_kB;
    if (max->rd_kBps < disk->rd_kBps)   max->rd_kBps = disk->rd_kBps;
    if (max->wr_kBps < disk->wr_kBps)   max->wr_kBps = disk->wr_kBps;
    if (max->util < disk->util)         max->util = disk->util;
    /* mean */
    struct ext_stats_disk mean_prev = *mean;
    mean->tps   = compute_mean(n, mean->tps, disk->tps);
    mean->rd_kB = compute_mean(n, mean->rd_kB, disk->rd_kB);
    mean->wr_kB = compute_mean(n, mean->wr_kB, disk->wr_kB);
    mean->rd_kBps = compute_mean(n, mean->rd_kBps, disk->rd_kBps);
    mean->wr_kBps = compute_mean(n, mean->wr_kBps, disk->wr_kBps);
    mean->wr_kBps = compute_mean(n, mean->wr_kBps, disk->wr_kBps);
    mean->util  = compute_mean(n, mean->util, disk->util);
    /* var */
    var->tps    = compute_var(n, var->tps, mean->tps, mean_prev.tps, disk->tps);
    var->rd_kB  = compute_var(n, var->rd_kB, mean->rd_kB, mean_prev.rd_kB, disk->rd_kB);
    var->wr_kB  = compute_var(n, var->wr_kB, mean->wr_kB, mean_prev.wr_kB, disk->wr_kB);
    var->rd_kBps  = compute_var(n, var->rd_kBps, mean->rd_kBps, mean_prev.rd_kBps, disk->rd_kBps);
    var->wr_kBps  = compute_var(n, var->wr_kBps, mean->wr_kBps, mean_prev.wr_kBps, disk->wr_kBps);
    var->util    = compute_var(n, var->util, mean->util, mean_prev.util, disk->util);
}


void update_ext_net_stats(int n,                        /* IN */
                          struct ext_stats_net *net,    /* IN */
                          struct ext_stats_net *min,    /* OUT */
                          struct ext_stats_net *max,    /* OUT */
                          struct ext_stats_net *mean,   /* OUT */
                          struct ext_stats_net *var)    /* OUT */
{
    /* min */
    if (net->rx_pktps < min->rx_pktps)  min->rx_pktps = net->rx_pktps;
    if (net->tx_pktps < min->tx_pktps)  min->tx_pktps = net->tx_pktps;
    if (net->rx_kBps < min->rx_kBps)    min->rx_kBps = net->rx_kBps;
    if (net->tx_kBps < min->tx_kBps)    min->tx_kBps = net->tx_kBps;
    if (net->util < min->util)          min->util = net->util;
    /* max */
    if (max->rx_pktps < net->rx_pktps)  max->rx_pktps = net->rx_pktps;
    if (max->tx_pktps < net->tx_pktps)  max->tx_pktps = net->tx_pktps;
    if (max->rx_kBps < net->rx_kBps)    max->rx_kBps = net->rx_kBps;
    if (max->tx_kBps < net->tx_kBps)    max->tx_kBps = net->tx_kBps;
    if (max->util < net->util)          max->util = net->util;
    /* mean */
    struct ext_stats_net mean_prev = *mean;
    mean->rx_pktps      = compute_mean(n, mean->rx_pktps, net->rx_pktps);
    mean->tx_pktps      = compute_mean(n, mean->tx_pktps, net->tx_pktps);
    mean->rx_kBps       = compute_mean(n, mean->rx_kBps, net->rx_kBps);
    mean->tx_kBps       = compute_mean(n, mean->tx_kBps, net->tx_kBps);
    mean->util          = compute_mean(n, mean->util, net->util);
    /* var */
    var->rx_pktps = compute_var(n, var->rx_pktps, mean->rx_pktps, mean_prev.rx_pktps, net->rx_pktps);
    var->tx_pktps = compute_var(n, var->tx_pktps, mean->tx_pktps, mean_prev.tx_pktps, net->tx_pktps);
    var->rx_kBps = compute_var(n, var->rx_kBps, mean->rx_kBps, mean_prev.rx_kBps, net->rx_kBps);
    var->tx_kBps = compute_var(n, var->tx_kBps, mean->tx_kBps, mean_prev.tx_kBps, net->tx_kBps);
    var->util = compute_var(n, var->util, mean->util, mean_prev.util, net->util);
}


void update_ext_mem_stats(int n,                        /* IN */
                          struct ext_stats_mem *mem,    /* IN */
                          struct ext_stats_mem *min,    /* OUT */
                          struct ext_stats_mem *max,    /* OUT */
                          struct ext_stats_mem *mean,   /* OUT */
                          struct ext_stats_mem *var)    /* OUT */
{
    /* min */
    if (mem->u.mem_used < min->u.mem_used)  min->u.mem_used = mem->u.mem_used;
    if (mem->util < min->util)  min->util = mem->util;
    /* max */
    if (max->u.mem_used < mem->u.mem_used)  max->u.mem_used = mem->u.mem_used;
    if (max->util < mem->util)  max->util = mem->util;
    /* mean */
    struct ext_stats_mem mean_prev = *mean;
    mean->u.mem_used_ = compute_mean(n, mean->u.mem_used_, (double)mem->u.mem_used);
    mean->util = compute_mean(n, mean->util, mem->util);
    /* var */
    var->u.mem_used_ = compute_var(n, var->u.mem_used_, mean->u.mem_used_, 
                                   mean_prev.u.mem_used_, (double)mem->u.mem_used);
    var->util = compute_var(n, var->util, mean->util, mean_prev.util, mem->util);
}

