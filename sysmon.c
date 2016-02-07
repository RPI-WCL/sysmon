#include "common.h"
#include "rd_stats.h"
#include "print_flags.h"
#include "utils.h"
#include "ext_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>     /* For ULONG_MAX */
#include <float.h>      /* For DBL_MAX */

#define INTERVAL_SEC    1
#define SEC_TO_MSEC     1000000
#define LOG_FILE        "./log"

/* local type definition */
struct stats {
    struct stats_cpu cpu;
    unsigned long long uptime, uptime0;
    struct stats_sysfs_disk disk;
    struct stats_net_dev net;
    struct stats_memory mem;    
};

struct ext_stats { /* extended stats */
    struct ext_stats_cpu cpu;
    struct ext_stats_disk disk;
    struct ext_stats_net net;
    struct ext_stats_mem mem;
};

/* base stats/time */
static struct stats base_stats;
static struct timespec base_time;

/* min/max/mean/var calculation for ext_stats */
static int num_samples;
static struct ext_stats min, max, mean, var;

/* log */
static FILE *fp = NULL;
static int realtime_logging = 0;
static unsigned int print_flag = UTIL_ALL;
static char *print_headers[]=
{/* CPU */
 "cpu_user", "cpu_nice", "cpu_sys", "cpu_idle", "cpu_iowait", 
 "cpu_user[%],cpu_nice[%],cpu_sys[%],cpu_idle[%],cpu_iowait[%]",
 /* Disk */
 "rd_ios", "wr_ios", "rd_sect", "wr_sect", "rd_ticks", "wr_ticks", "tot_ticks",
 "tps", "rd_kB", "wr_kB", "rd_kBps", "wr_kBps", "disk_util[%]",
 /* Net */
 "rx_pkt", "tx_pkt", "rx_kB", "tx_kB", 
 "rx_pktps","tx_pktps", "rx_kBps", "tx_kBps", "net_util[%]",
 /* Memory */
 "tlmkb", 
 "mem_used", "mem_util[%]"};

#define FPRINTF_DATA(fp, print_flag, flag, fmt, val)     if (print_flag & flag) { \
    fprintf(fp, fmt, val); \
    if ((flag << 1) <= print_flag) fprintf(fp, ",");   \
    }


void read_stats(struct stats *st) 
{
    read_stat_cpu(&st->cpu, 2, &st->uptime, &st->uptime0);
    read_sysfs_disk(&st->disk, "sda");
    read_net_dev(&st->net, 1);
    read_meminfo(&st->mem);
}


#if 0
/* st1 += st2 */
void add_ext_stats(struct ext_stats *st1, struct ext_stats *st2) 
{
    int i;

    for (i = 0; i < CPU_STATES; i++)
        st1->cpu.utils[i] += st2->cpu.utils[i];

    st1->disk.tps       += st2->disk.tps;
    st1->disk.rd_kBps   += st2->disk.rd_kBps;
    st1->disk.wr_kBps   += st2->disk.wr_kBps;
    st1->disk.util      += st2->disk.util;

    st1->net.rx_pktps   += st2->net.rx_pktps;
    st1->net.tx_pktps   += st2->net.tx_pktps;
    st1->net.rx_kBps    += st2->net.rx_kBps;
    st1->net.tx_kBps    += st2->net.tx_kBps;
    st1->net.util       += st2->net.util;

    st1->mem.mem_used   += st2->mem.mem_used;
    st1->mem.util       += st2->mem.util;
}
#endif


void compute_ext_stats(unsigned long long interval,
                       struct stats *st, struct stats *st_prev, 
                       struct ext_stats *ext_st)
{
    compute_ext_cpu_stats(&st->cpu, &st_prev->cpu, &ext_st->cpu);
    compute_ext_sysfs_disk_stats(&st->disk, &st_prev->disk, interval, 2 /* KB */, &ext_st->disk);
    compute_ext_net_stats(&st->net, &st_prev->net, interval, &ext_st->net);
    compute_ext_mem_stats(&st->mem, &ext_st->mem);
}

void update_ext_stats(int num,                  /* IN */
                      struct ext_stats *st,     /* IN */                         
                      struct ext_stats *min,    /* OUT */
                      struct ext_stats *max,    /* OUT */
                      struct ext_stats *mean,   /* OUT */
                      struct ext_stats *var)    /* OUT */
{
    update_ext_cpu_stats(num, &st->cpu, &min->cpu, &max->cpu, &mean->cpu, &var->cpu);
    update_ext_disk_stats(num, &st->disk, &min->disk, &max->disk, &mean->disk, &var->disk);
    update_ext_net_stats(num, &st->net, &min->net, &max->net, &mean->net, &var->net);
    update_ext_mem_stats(num, &st->mem, &min->mem, &max->mem, &mean->mem, &var->mem);
}


void set_max_ext_stats(struct ext_stats *st)
{
    int i;
    
    /* TODO: maybe should do this in ext_stat.c */
    
    /* CPU */
    for (i = 0; i < CPU_STATES; i++)
        st->cpu.utils[i] = DBL_MAX;
    /* Disk */
    st->disk.tps= DBL_MAX;
    st->disk.rd_kB = st->disk.wr_kB = DBL_MAX;
    st->disk.rd_kBps = st->disk.wr_kBps = DBL_MAX;
    st->disk.util = DBL_MAX;
    /* Net */
    st->net.rx_pktps = st->net.tx_pktps = DBL_MAX;
    st->net.rx_kBps = st->net.rx_kBps = DBL_MAX;
    st->net.util = DBL_MAX;
    /* Memory */
    st->mem.u.mem_used = ULONG_MAX;
    st->mem.util = DBL_MAX;
}


int fprint_header(FILE *fp, unsigned int f)
{
    if (fp == NULL)
        return EXIT_FAILURE;

    fprintf(fp, "    time: ");
    
    int i = 0;
    while (f) {
        if (f & 0x00000001) {
            fprintf(fp, "%s", print_headers[i]);
            if (1 < f)  fprintf(fp, ",");
        }
        f = f >> 1;
        i++;
    }
    fprintf(fp, "\n");

    return EXIT_SUCCESS;
}


double get_elapsed_time(struct timespec* base)
{
    // returns elapsed time in sec
    struct timespec now;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - base->tv_sec) + (double)(now.tv_nsec - base->tv_nsec) / 1.0e9;;
    
    return elapsed;
}


int fprint_stats(FILE *fp, unsigned int print_flag, 
                 struct stats *st, struct ext_stats *ext_st)
{
    if (fp == NULL)
        return EXIT_FAILURE;

    fprintf(fp, "%8.3f: ", get_elapsed_time(&base_time));

    if (print_flag & CPU_ALL) {
        FPRINTF_DATA(fp, print_flag, CPU_USER,  "%llu", st->cpu.cpu_user);
        FPRINTF_DATA(fp, print_flag, CPU_NICE,  "%llu", st->cpu.cpu_user);
        FPRINTF_DATA(fp, print_flag, CPU_SYS,   "%llu", st->cpu.cpu_sys);
        FPRINTF_DATA(fp, print_flag, CPU_IDLE,  "%llu", st->cpu.cpu_idle);
        FPRINTF_DATA(fp, print_flag, CPU_IOWAIT,"%llu", st->cpu.cpu_iowait);
        if (print_flag & CPU_UTIL) {
            fprintf(fp, "%.3f,%.3f,%.3f,%.3f,%.3f", 
                    ext_st->cpu.utils[0], ext_st->cpu.utils[1], ext_st->cpu.utils[2], 
                    ext_st->cpu.utils[3], ext_st->cpu.utils[4]);
            if (CPU_UTIL < print_flag) fprintf(fp, ",");
        }
    }
    
    if (print_flag & DISK_ALL) {
        FPRINTF_DATA(fp, print_flag, RD_IOS,    "%lu", st->disk.rd_ios);
        FPRINTF_DATA(fp, print_flag, WR_IOS,    "%lu", st->disk.wr_ios);
        FPRINTF_DATA(fp, print_flag, RD_SECT,   "%lu", st->disk.rd_sectors);
        FPRINTF_DATA(fp, print_flag, WR_SECT,   "%lu", st->disk.wr_sectors);
        FPRINTF_DATA(fp, print_flag, RD_TICKS,  "%u", st->disk.rd_ticks);
        FPRINTF_DATA(fp, print_flag, WR_TICKS,  "%u", st->disk.wr_ticks);
        FPRINTF_DATA(fp, print_flag, TOT_TICKS, "%u", st->disk.tot_ticks);

        FPRINTF_DATA(fp, print_flag, TPS,       "%.3f", ext_st->disk.tps);
        FPRINTF_DATA(fp, print_flag, RD_KBYTES, "%.3f", ext_st->disk.rd_kB);
        FPRINTF_DATA(fp, print_flag, WR_KBYTES, "%.3f", ext_st->disk.wr_kB);
        FPRINTF_DATA(fp, print_flag, RD_KBPS,   "%.3f", ext_st->disk.rd_kBps);
        FPRINTF_DATA(fp, print_flag, WR_KBPS,   "%.3f", ext_st->disk.wr_kBps);
        FPRINTF_DATA(fp, print_flag, DISK_UTIL, "%.3f", ext_st->disk.util);
    }

    if (print_flag & NET_ALL) {
        FPRINTF_DATA(fp, print_flag, RX_PACKETS,"%llu", st->net.rx_packets);
        FPRINTF_DATA(fp, print_flag, TX_PACKETS,"%llu", st->net.tx_packets);
        FPRINTF_DATA(fp, print_flag, RX_KBYTES, "%.3f", (double)st->net.rx_bytes / 1024);
        FPRINTF_DATA(fp, print_flag, TX_KBYTES, "%.3f", (double)st->net.tx_bytes / 1024);

        FPRINTF_DATA(fp, print_flag, RX_PKTPS,  "%.3f", ext_st->net.rx_pktps);
        FPRINTF_DATA(fp, print_flag, TX_PKTPS,  "%.3f", ext_st->net.tx_pktps);
        FPRINTF_DATA(fp, print_flag, RX_KBPS,   "%.3f", ext_st->net.rx_kBps);
        FPRINTF_DATA(fp, print_flag, TX_KBPS,   "%.3f", ext_st->net.tx_kBps);
        FPRINTF_DATA(fp, print_flag, NET_UTIL,  "%.3f", ext_st->net.util);
    }

    if (print_flag & MEM_ALL) {
        FPRINTF_DATA(fp, print_flag, MEM_TOTAL, "%lu", st->mem.tlmkb);
        if (print_flag & MEM_USED_DBL) {
            FPRINTF_DATA(fp, print_flag, MEM_USED,  "%.3f", ext_st->mem.u.mem_used_); /* mean/var */
        }
        else {
            FPRINTF_DATA(fp, print_flag, MEM_USED,  "%llu", ext_st->mem.u.mem_used);
        }
        FPRINTF_DATA(fp, print_flag, MEM_UTIL,  "%.3f", ext_st->mem.util);
    }

    fprintf(fp, "\n");
    
    return EXIT_SUCCESS;
}


/* st1 -= st2 */
void subst_stats(struct stats *st1, struct stats *st2)
{
    st1->cpu.cpu_user   -= st2->cpu.cpu_user;
    st1->cpu.cpu_nice   -= st2->cpu.cpu_nice;
    st1->cpu.cpu_sys    -= st2->cpu.cpu_sys;
    st1->cpu.cpu_idle   -= st2->cpu.cpu_idle;
    st1->cpu.cpu_iowait -= st2->cpu.cpu_iowait;

    st1->disk.rd_ios    -= st2->disk.rd_ios;
    st1->disk.wr_ios    -= st2->disk.wr_ios;
    st1->disk.rd_sectors-= st2->disk.rd_sectors;
    st1->disk.wr_sectors-= st2->disk.wr_sectors;
    st1->disk.rd_ticks  -= st2->disk.rd_ticks;
    st1->disk.wr_ticks  -= st2->disk.wr_ticks;
    st1->disk.tot_ticks -= st2->disk.tot_ticks;

    st1->net.rx_packets -= st2->net.rx_packets;
    st1->net.tx_packets -= st2->net.tx_packets;
    st1->net.rx_bytes   -= st2->net.rx_bytes;
    st1->net.tx_bytes   -= st2->net.tx_bytes;

    /* For memory, does not make sense to compare st1 and st2 */
} 


/* "pkill --signal 2 sysmon" will be captured here. */
void signal_callback_handler(int signum)
{
    struct stats st;
    struct ext_stats ext_st;
    unsigned long long interval;

    printf("\nCaught signal %d\n", signum);

    /* Overall stats calculation */
    read_stats(&st);
    interval = get_interval(base_stats.uptime, st.uptime);

    /* Overall stats output */
    fprintf(fp, "Mean:\n");
    #if 1
    subst_stats(&st, &base_stats);      /* st -= base_stats */
    fprint_stats(fp, print_flag | MEM_USED_DBL, &st, &mean);
    #else
    compute_ext_stats(interval, &st, &base_stats, &ext_st);
    ext_st.mem = mean.mem;
    subst_stats(&st, &base_stats);      /* st -= base_stats */
    fprint_stats(fp, print_flag | MEM_USED_DBL, &st, &ext_st);
    #endif

    #if 0
    fprintf(fp, "Minimum:\n");
    fprint_stats(fp, print_flag, &st, &min);
    fprintf(fp, "Maximum:\n");
    fprint_stats(fp, print_flag, &st, &max);
    fprintf(fp, "Variance:\n");
    fprint_stats(fp, print_flag | MEM_USED_DBL, &st, &var);
    #endif

    /* All done! */
    #ifndef DEBUG
    fclose(fp);
    #endif
    exit(signum);
}


int main(int argc, char* argv[])
{
    struct stats st, st_prev;
    struct ext_stats ext_st;
    unsigned long long interval;
    char *logfile = LOG_FILE;

    /* Usage: ./sysmon [real-time logging switch] [log filename] */
    realtime_logging = (1 < argc && (strcmp("1", argv[1]) == 0));
    if (2 < argc)
        logfile = argv[2];
    #ifndef DEBUG
    if ((fp = fopen(logfile, "w+")) == NULL) {
        fprintf(stderr, "opening logfile failed\n");
        exit(EXIT_FAILURE);
    }
    #else
    fp = stdout;
    #endif

    /* Initialization */
    signal(SIGINT, signal_callback_handler); /* Set up a signal handler */
    read_stats(&base_stats);
    clock_gettime(CLOCK_MONOTONIC, &base_time);
    /* Obtain speed/duplex info for eth0 */
    read_net_dev(&st.net, 1);
    read_if_info(&st.net, 1); 
    base_stats.net.speed = st.net.speed;
    base_stats.net.duplex = st.net.duplex;
    st_prev = base_stats;
    /* Initialize variables for min/max/mean/var computation */
    num_samples = 1;
    set_max_ext_stats(&min);
    memset((void *)&max, 0x00, sizeof(struct ext_stats));
    memset((void *)&mean, 0x00, sizeof(struct ext_stats));
    memset((void *)&var, 0x00, sizeof(struct ext_stats));
    /* For real-time logging */
    if (realtime_logging) fprint_header(fp, print_flag);
    /* Get HZ */
    get_HZ();

    while (1) { /* Anyways, we record stats for average computation */
        usleep(INTERVAL_SEC * SEC_TO_MSEC);
        read_stats(&st);
        interval = get_interval(st_prev.uptime, st.uptime);

        /* Compute extended stats from raw stats */
        compute_ext_stats(interval, &st, &st_prev, &ext_st);
        update_ext_stats(num_samples, &ext_st, &min, &max, &mean, &var);
        if (realtime_logging) fprint_stats(fp, print_flag, &st, &ext_st);
        
        /* Update stats (min/max/mean/var) for ext_stats */
        st_prev = st;
        num_samples++;
    }

    /* Program never gets here */
    printf("Waiting for the SIGINT signal\n");
    pause();

    return EXIT_SUCCESS;
}

