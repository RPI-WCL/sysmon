#ifndef _UTILS_H
#define _UTILS_H

#include "rd_stats.h"

long percentages(int cnt, int *out, long *new, long *old, long *diffs);
double compute_ifutil(struct stats_net_dev *st_net_dev, double rx, double tx);

#endif /* #ifndef _UTILS_H */
