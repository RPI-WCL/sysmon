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
    ext_disk->rd_kbps = S_VALUE(disk_prev->rd_sectors, disk_curr->rd_sectors, interval) / factor;
    ext_disk->wr_kbps = S_VALUE(disk_prev->wr_sectors, disk_curr->wr_sectors, interval) / factor;
    #if 0
    ext_disk->rd_kb   = (unsigned long long) rd_sec / factor;
	ext_disk->wr_kb   = (unsigned long long) wr_sec / factor;
    #endif
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

    ext_net->rx_kbps = rx_kb / 1024;
    ext_net->tx_kbps = tx_kb/ 1024;
    ext_net->util = compute_ifutil(net_curr, rx_kb, tx_kb);
}


void compute_ext_mem_stats(struct stats_memory *mem,            /* IN */
                           struct ext_stats_mem *ext_mem)       /* OUT */
{
    ext_mem->mem_used = mem->tlmkb - mem->frmkb;
    ext_mem->util = (double)ext_mem->mem_used / mem->tlmkb;
}
