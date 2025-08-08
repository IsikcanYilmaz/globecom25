#ifndef IPERF_PKT_H
#define IPERF_PKT_H

#define IPERF_MAX_PKTS_IN_ONE_BULK_REQ 15

typedef enum {
  IPERF_PAYLOAD = 0x1,
  IPERF_PKT_REQ,
  IPERF_PKT_BULK_REQ,
  IPERF_PKT_RESP,
  IPERF_RX_BITMAP,
  IPERF_ECHO_CALL,
  IPERF_ECHO_RESP,
  IPERF_CONFIG_SYNC,
  IPERF_CTRL_MSG_MAX
} IperfMsgType_e;

typedef struct {
  uint8_t msgType; // 1
  uint8_t plSize;  // 1 
  uint16_t seqNo;  // 2
  uint8_t payload[]; // *
} __attribute__((packed)) IperfUdpPkt_t;

// All structs below go to the payload of the UdpPkt_t

typedef struct {
  uint8_t mode; // 1
  uint16_t payloadSizeBytes; // 2
  uint32_t delayUs; // 4
  uint32_t transferSizeBytes; // 4
  uint16_t numPktsToTransfer; // 2
  uint8_t numCacheBlocks; // 1
  uint32_t interestDelayUs; // 4
  uint32_t expectationDelayUs; // 4
  bool cache; // 1
  bool code; // 1
} __attribute__((packed)) IperfConfigPayload_t;

typedef struct { // TODO
  uint16_t seqNo;  // 2
} __attribute__((packed)) IperfInterest_t;

typedef struct {
  uint16_t len; // 2
  uint16_t arr[]; // *
} __attribute__((packed)) IperfBulkInterest_t;

typedef struct {
  uint16_t pktOffset; // 2
  uint8_t len; // 1 // todo do we want this? 
  uint8_t bitmap[];
} __attribute__((packed)) IperfReceivedPktBitmap_t;

#endif
