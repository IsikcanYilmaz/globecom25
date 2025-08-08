For the demo (and potentially general experimentation work) we need to work in a relatively small area, and we may want things to happen slower than normal for demo reasons. This means

- We may want nodes to talk to one another thru multiple hops but they don't need to. For this we lower our nodes' transmission powers and add RSSI limits to our network drivers such that if $pktRssi < $rssiLimit : drop packet

- We want network operations to have delays in between. For this we add a "Fake latency"

Both of these are doable through shell commands or settable in demo_throttlers.c.

However, in order to enable RSSI limiting, one needs to #define it or add it as a CFLAG. i.e. -DJON_RSSI_LIMITING=-60 to set it to -60

Same for fake latency. This is avail only for CCN-Lite. -DJON_FAKE_LATENCY_MS=200

One can also set default tx power like this, however it doesnt need to be set to be enabled. Default is -20. -DJON_DEFAULT_TX_POWER=-20

Throttler_Init() needs to be called in your main() before you do anything.
