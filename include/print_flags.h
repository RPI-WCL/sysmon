#ifndef _PRINT_FLAGS_H
#define _PRINT_FLAGS_H

/* Print flag definitions */

/* CPU: basic stats */
#define CPU_USER        0x00000001
#define	CPU_NICE        0x00000002
#define CPU_SYS         0x00000004
#define CPU_IDLE        0x00000008
#define CPU_IOWAIT      0x00000010
/* CPU: processed stats */
#define CPU_UTIL        0x00000020      /* printing all of user, nice, ... */
#define CPU_ALL         0x0000003F

/* Disk: basic stats */
#define RD_IOS          0x00000040
#define WR_IOS          0x00000080
#define RD_SECT         0x00000100
#define WR_SECT         0x00000200
#define RD_TICKS        0x00000400
#define WR_TICKS        0x00000800
#define TOT_TICKS       0x00001000
/* Disk: processed stats */
#define TPS             0x00002000
#define RD_KB           0x00004000
#define WR_KB           0x00008000
#define RD_KBPS         0x00010000
#define WR_KBPS         0x00020000
#define DISK_UTIL       0x00040000
#define DISK_ALL        0x0007FFC0

/* Net: basic stats */
#define RX_PACKETS      0x00080000
#define TX_PACKETS      0x00100000
#define RX_BYTES        0x00200000
#define TX_BYTES        0x00400000
/* Net: processed stats */
#define RX_PKTPS        0x00800000
#define TX_PKTPS        0x01000000
#define RX_KBPS         0x02000000
#define TX_KBPS         0x04000000
#define NET_UTIL        0x08000000
#define NET_ALL         0x0FF80000

/* Memory: basic stats */
#define MEM_TOTAL       0x10000000
#define MEM_USED        0x20000000
#define MEM_UTIL        0x40000000
#define MEM_ALL         0x70000000

#define PRINT_ALL       0x7FFFFFFF

#endif /* #ifndef _PRINT_FLAGS_H */
