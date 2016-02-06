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

/* base stats */
static struct stats base_stats;

/* average calculation */
static int num_samples;
static struct ext_stats avg_ext_stats;

/* log */
static FILE *fp = NULL;
static int realtime_logging = 0;
static unsigned int print_flag = DISK_ALL;
static char *print_headers[]=
{/* CPU */
 "cpu_user", "cpu_nice", "cpu_sys", "cpu_idle", "cpu_iowait", 
 "cpu_user[%],cpu_nice[%],cpu_sys[%],cpu_idle[%],cpu_iowait[%]",
 /* Disk */
 "rd_ios", "wr_ios", "rd_sectors", "wr_sectors", "rd_ticks", "wr_ticks",
 "tot_ticks", "tps", "rd_kb", "wr_kb", "rd_kbps", "wr_kbps", "disk_util[%]",
 /* Net */
 "rx_pkt", "tx_pkt", "rx_bytes", "tx_bytes", "rx_pktps","tx_pktps", "rx_kbps", "tx_kbps",
 "net_util[%]",
 /* Memory */
 "tlmkb", "mem_used", "mem_util[%]"};

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


/* st1 += st2 */
void add_ext_stats(struct ext_stats *st1, struct ext_stats *st2) 
{
    int i;

    for (i = 0; i < CPU_STATES; i++)
        st1->cpu.utils[i] += st2->cpu.utils[i];

    st1->disk.tps       += st2->disk.tps;
    st1->disk.rd_kbps   += st2->disk.rd_kbps;
    st1->disk.wr_kbps   += st2->disk.wr_kbps;
    st1->disk.util      += st2->disk.util;

    st1->net.rx_pktps   += st2->net.rx_pktps;
    st1->net.tx_pktps   += st2->net.tx_pktps;
    st1->net.rx_kbps    += st2->net.rx_kbps;
    st1->net.tx_kbps    += st2->net.tx_kbps;
    st1->net.util       += st2->net.util;

    st1->mem.mem_used   += st2->mem.mem_used;
    st1->mem.util       += st2->mem.util;
}


void compute_ext_stats(unsigned long long interval,
                       struct stats *st, struct stats *st_prev, 
                       struct ext_stats *ext_st)
{
    compute_ext_cpu_stats(&st->cpu, &st_prev->cpu, &ext_st->cpu);
    compute_ext_sysfs_disk_stats(&st->disk, &st_prev->disk, interval, 2 /* KB */, &ext_st->disk);
    compute_ext_net_stats(&st->net, &st_prev->net, interval, &ext_st->net);
    compute_ext_mem_stats(&st->mem, &ext_st->mem);

    add_ext_stats(&avg_ext_stats, ext_st);
}


int fprint_header(FILE *fp, unsigned int f)
{
    if (fp == NULL)
        return EXIT_FAILURE;
    
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


int fprint_stats(FILE *fp, unsigned int print_flag, 
                 struct stats *st, struct ext_stats *ext_st)
{
    if (fp == NULL)
        return EXIT_FAILURE;

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
        FPRINTF_DATA(fp, print_flag, RD_KBPS,   "%.3f", ext_st->disk.rd_kbps);
        FPRINTF_DATA(fp, print_flag, WR_KBPS,   "%.3f", ext_st->disk.wr_kbps);
        FPRINTF_DATA(fp, print_flag, DISK_UTIL, "%.3f", ext_st->disk.util);
    }

    if (print_flag & NET_ALL) {
        FPRINTF_DATA(fp, print_flag, RX_PACKETS,"%llu", st->net.rx_packets);
        FPRINTF_DATA(fp, print_flag, TX_PACKETS,"%llu", st->net.tx_packets);
        FPRINTF_DATA(fp, print_flag, RX_BYTES,  "%llu", st->net.rx_bytes);
        FPRINTF_DATA(fp, print_flag, TX_BYTES,  "%llu", st->net.tx_bytes);

        FPRINTF_DATA(fp, print_flag, RX_PKTPS,  "%.3f", ext_st->net.rx_pktps);
        FPRINTF_DATA(fp, print_flag, TX_PKTPS,  "%.3f", ext_st->net.tx_pktps);
        FPRINTF_DATA(fp, print_flag, RX_KBPS,   "%.3f", ext_st->net.rx_kbps);
        FPRINTF_DATA(fp, print_flag, TX_KBPS,   "%.3f", ext_st->net.tx_kbps);
        FPRINTF_DATA(fp, print_flag, NET_UTIL,  "%.3f", ext_st->net.util);
    }

    if (print_flag & MEM_ALL) {
        FPRINTF_DATA(fp, print_flag, MEM_TOTAL, "%lu", st->mem.tlmkb);
        FPRINTF_DATA(fp, print_flag, MEM_USED,  "%lu", ext_st->mem.mem_used);
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


void compute_avg_ext_stats(int num_samples, struct ext_stats *ext_st)
{
    int i;

    for (i = 0; i < CPU_STATES; i++)
        ext_st->cpu.utils[i]    /= num_samples;

    ext_st->disk.tps            /= num_samples;
    ext_st->disk.rd_kbps        /= num_samples;
    ext_st->disk.wr_kbps        /= num_samples;
    ext_st->disk.util           /= num_samples;

    ext_st->net.rx_pktps        /= num_samples;
    ext_st->net.tx_pktps        /= num_samples;
    ext_st->net.rx_kbps         /= num_samples;
    ext_st->net.tx_kbps         /= num_samples;
    ext_st->net.util            /= num_samples;

    ext_st->mem.util            /= num_samples;
}


/* "pkill --signal 2 sysmon" will be captured here. */
void signal_callback_handler(int signum)
{
    struct stats st;

    printf("\nCaught signal %d\n", signum);

    /* Overall stats calculation */
    read_stats(&st);                    /* Read one last time */
    subst_stats(&st, &base_stats);      /* st -= base_stats */
    compute_avg_ext_stats(num_samples, &avg_ext_stats);

    /* Overall stats ouput */
    fprintf(fp, "\n");
    fprint_header(fp, print_flag);
    fprint_stats(fp, print_flag, &st, &avg_ext_stats);

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
    /* Obtain speed/duplex info for eth0 */
    read_net_dev(&st.net, 1);
    read_if_info(&st.net, 1); 
    base_stats.net.speed = st.net.speed;
    base_stats.net.duplex = st.net.duplex;
    st_prev = base_stats;
    /* Initialize variables for average computation */
    num_samples = 0;
    bzero((void *)&avg_ext_stats, sizeof(struct ext_stats));
    /* For real-time logging */
    if (realtime_logging) fprint_header(fp, print_flag);
    /* Get HZ */
    get_HZ();

    while (1) { /* Anyways, we record stats for average computation */
        usleep(INTERVAL_SEC * SEC_TO_MSEC);
        read_stats(&st);
        interval = get_interval(st_prev.uptime, st.uptime);

        /* Process sampled stats*/
        compute_ext_stats(interval, &st, &st_prev, &ext_st);
        if (realtime_logging) fprint_stats(fp, print_flag, &st, &ext_st);
        
        /* Update stats */
        st_prev = st;
        num_samples++;
    }

    /* Program never gets here */
    printf("Waiting for the SIGINT signal\n");
    pause();

    return EXIT_SUCCESS;
}

