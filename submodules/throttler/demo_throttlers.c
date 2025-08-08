#include "shell.h"
#include <stdbool.h>
#include <stdlib.h>
#include "net/gnrc.h"
#include "net/gnrc/netif.h"
#include "net/netopt.h"

#include "demo_throttlers.h"

// TODO this and the settings .c file should merge

#ifdef JON_DEFAULT_TX_POWER
#define DEFAULT_TX_POWER (JON_DEFAULT_TX_POWER)
#else
#define DEFAULT_TX_POWER (-20)
#endif

#ifdef JON_RSSI_LIMITING
#define DEFAULT_RSSI_LIMITOR (JON_RSSI_LIMITING)
extern int rssiLimitor;
extern bool rssiPrint;
#endif

#ifdef JON_FAKE_LATENCY_MS
#define DEFAULT_FAKE_LATENCY_MS (JON_FAKE_LATENCY_MS) // TODO maybe remove these settings from the makefile 
extern uint32_t fakeLatencyMs;
#endif

#ifdef JON_802154_MAX_RETRANS
extern uint8_t jon802154MaxRetrans; 
#endif

int16_t txPower;
netif_t *mainIface;
char netifName[CONFIG_NETIF_NAMELENMAX];

void Throttler_Init(void)
{
#ifdef JON_RSSI_LIMITING
  rssiLimitor = DEFAULT_RSSI_LIMITOR;
#endif
#ifdef JON_FAKE_LATENCY_MS
  fakeLatencyMs = DEFAULT_FAKE_LATENCY_MS;
#endif
  txPower = DEFAULT_TX_POWER;

  // Assuming we have one network interface, return the name of the "last" one:
  mainIface = netif_iter(NULL);
  netif_get_name(mainIface, netifName);
  Throttler_SetTxPower(txPower);
  printf("Throttler initialized for netif %s\n", netifName);
}

void Throttler_SetRssiLimitor(int rssi)
{
#ifdef JON_RSSI_LIMITING
  rssiLimitor = rssi;
  printf("Rssi limitor set to %d\n", rssi);
#else
  printf("RSSI LIMITING NOT ENABLED!\n");
#endif
}

int Throttler_GetRssiLimitor(void)
{
#ifdef JON_RSSI_LIMITING
  return rssiLimitor;
#else 
  printf("RSSI LIMITING NOT ENABLED!\n");
  return 0;
#endif
}

int Throttler_SetTxPower(int16_t txpower)
{
  if (netif_set_opt(mainIface, NETOPT_TX_POWER, 0, &txpower, sizeof(int16_t)) < 0)
  {
    printf("%s unable to set tx power to %d\n", __FUNCTION__, txpower);
    return 1;
  }
  txPower = txpower;
  printf("%s set tx power to %d\n", __FUNCTION__, txpower);
  return 0;
}

int Throttler_GetTxPower(void)
{
  return 0; // TODO
}

int Throttler_CmdSetTxPower(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("Usage: txpower <int>\n");
    return 1;
  }
  return Throttler_SetTxPower(atoi(argv[1]));
}

int Throttler_CmdGetTxPower(int argc, char **argv)
{
  printf("Iface: %s\n", netifName);
  printf("%d dBm\n", txPower);
  return 0;
}

// SHELL CMDS
int setRssiLimitor(int argc, char **argv)
{
#ifdef JON_RSSI_LIMITING
  if (argc != 2)
  {
    printf("Usage: setrssi <int>\n");
    return 1;
  }
  Throttler_SetRssiLimitor(atoi(argv[1]));
#endif
  return 0;
}

int noRssiLimitor(int argc, char **argv)
{
  Throttler_SetRssiLimitor(-100);
}

int getRssiLimitor(int argc, char **argv)
{
  #ifdef JON_RSSI_LIMITING
  printf("%d\n", Throttler_GetRssiLimitor());
  #endif
  return 0;
}

int toggleRssiPrint(int argc, char **argv)
{
  #ifdef JON_RSSI_LIMITING
  rssiPrint = !rssiPrint;
  printf("%d\n", rssiPrint);
  #endif
  return 0;
}

// TODO JON add ifdef like above
#ifdef JON_FAKE_LATENCY_MS
int setFakeLatency(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("Usage: setfakelat <int>\n");
    return 1;
  }
  fakeLatencyMs = atoi(argv[1]);
  printf("Fake latency set to %d\n", fakeLatencyMs);
  return 0;
}

int getFakeLatency(int argc, char **argv)
{
  printf("%d\n", fakeLatencyMs);
  return 0;
}
#endif

#ifdef JON_802154_MAX_RETRANS
int setL2Retrans(int argc, char **argv)
{
  if (argc < 2)
  {
    printf("Usage: setretrans <int>\n");
    return 1;
  }
  jon802154MaxRetrans = atoi(argv[1]);
  printf("L2 retransmissions set to %d\n", jon802154MaxRetrans);
  return 0;
}

int getL2Retrans(int argc, char **argv)
{
  printf("%d\n", jon802154MaxRetrans);
}

#endif

#ifdef THROTTLERS_SHELL_COMMANDS

#ifdef JON_FAKE_LATENCY_MS
SHELL_COMMAND(setfakelat, "setfakelat <ms>", setFakeLatency);
SHELL_COMMAND(getfakelat, "getfakelat", getFakeLatency);
#endif

#ifdef JON_RSSI_LIMITING
SHELL_COMMAND(setrssi, "setrssi <dbm>. pkts at rssis worse than this will be dropped", setRssiLimitor);
SHELL_COMMAND(getrssi, "getrssi", getRssiLimitor);
SHELL_COMMAND(norssi, "norssi removes rssi limit", noRssiLimitor)
SHELL_COMMAND(rssiprint, "rssiprint toggle rssi limitor prints", toggleRssiPrint);
#endif

#ifdef JON_802154_MAX_RETRANS
SHELL_COMMAND(getretrans, "get max l2 frame retransmissions", getL2Retrans);
SHELL_COMMAND(setretrans, "set max l2 frame retransmissions. 0 means disabled", setL2Retrans);
#endif

SHELL_COMMAND(setpwr, "setpwr <dbm>. sets tx power", Throttler_CmdSetTxPower);
SHELL_COMMAND(getpwr, "getpwr prints tx power", Throttler_CmdGetTxPower);

#endif
