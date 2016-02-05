#include "rd_stats.h"

/* Source: utils.c (top-3.8beta1)
 *  percentages(cnt, out, new, old, diffs) - calculate percentage change
 *	between array "old" and "new", putting the percentages i "out".
 *	"cnt" is size of each array and "diffs" is used for scratch space.
 *	The array "old" is updated on each call.
 *	The routine assumes modulo arithmetic.  This function is especially
 *	useful on BSD mchines for calculating cpu state percentages.
 */
long percentages(int cnt, int *out, long *new, long *old, long *diffs)
{
    register int i;
    register long change;
    register long total_change;
    register long *dp;
    long half_total;

    /* initialization */
    total_change = 0;
    dp = diffs;

    /* calculate changes for each state and the overall change */
    for (i = 0; i < cnt; i++) {
        if ((change = *new - *old) < 0) {
            /* this only happens when the counter wraps */
            change = (int)
                ((unsigned long)*new-(unsigned long)*old);
        }
        total_change += (*dp++ = change);
        *old++ = *new++;
    }

    /* avoid divide by zero potential */
    if (total_change == 0) {
        total_change = 1;
    }

    /* calculate percentages based on overall change, rounding up */
    half_total = total_change / 2l;
    for (i = 0; i < cnt; i++) {
        *out++ = (int)((*diffs++ * 1000 + half_total) / total_change);
    }

    /* return the total in case the caller wants to use it */
    return(total_change);
}


/* Source: "compute_ifutil" in sa_common.c (sysstat)
 ***************************************************************************
 * Compute network interface utilization.
 *
 * IN:
 * @st_net_dev	Structure with network interface stats.
 * @rx		Number of bytes received per second.
 * @tx		Number of bytes transmitted per second.
 *
 * RETURNS:
 * NIC utilization (0-100%).
 ***************************************************************************
 */
double compute_ifutil(struct stats_net_dev *st_net_dev, double rx, double tx)
{
	unsigned long long speed;

	if (st_net_dev->speed) {

		speed = (unsigned long long) st_net_dev->speed * 1000000;

		if (st_net_dev->duplex == C_DUPLEX_FULL) {
			/* Full duplex */
			if (rx > tx) {
				return (rx * 800 / speed);
			}
			else {
				return (tx * 800 / speed);
			}
		}
		else {
			/* Half duplex */
			return ((rx + tx) * 800 / speed);
		}
	}

	return 0;
}
