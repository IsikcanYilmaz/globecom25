
void Throttler_Init(void);
void Throttler_SetRssiLimitor(int rssi);
int Throttler_GetRssiLimitor(void);
int Throttler_SetTxPower(int16_t txpower);
int Throttler_GetTxPower(void);

int Throttler_CmdSetTxPower(int argc, char **argv);
int setRssiLimitor(int argc, char **argv); // TODO make these names match the rest
int getRssiLimitor(int argc, char **argv);
int setRssiPrint(int argc, char **argv);


int setFakeLatency(int argc, char **argv);
int getFakeLatency(int argc, char **argv);
