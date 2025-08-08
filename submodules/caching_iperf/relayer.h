#include "net/gnrc.h"
#include <stdbool.h>

bool Iperf_RelayerIntercept(gnrc_pktsnip_t *pkt);
void Iperf_PrintCache(void);
void *Iperf_RelayerThread(void *arg);
