#include <stdbool.h>

#include "net/gnrc.h"
#include "net/gnrc/nettype.h"
#include "iperf_pkt.h"

#define IPERF_TOTAL_TRANSMISSION_SIZE_MAX (4096)
#define IPERF_BUFFER_SIZE_BYTES 600
#define IPERF_MSG_QUEUE_SIZE 32
#define IPERF_DEFAULT_PORT (1)
#define IPERF_PAYLOAD_DEFAULT_SIZE_BYTES 32
#define IPERF_PAYLOAD_MAX_SIZE_BYTES 512
#define IPERF_DEFAULT_DELAY_US 1000000
#define IPERF_DEFAULT_PKT_PER_SECOND // TODO
#define IPERF_DEFAULT_TRANSFER_SIZE_BYTES (IPERF_PAYLOAD_DEFAULT_SIZE_BYTES * 16) // 512 bytes
#define IPERF_DEFAULT_TRANSFER_TIME_US (IPERF_DEFAULT_DELAY_US * 10) // 10 secs

typedef enum
{
  IPERF_IPC_MSG_START = 0xfff0,
  IPERF_IPC_MSG_SEND_FILE,
  IPERF_IPC_MSG_SEND_REQ,
  IPERF_IPC_MSG_IDLE,
  IPERF_IPC_MSG_STOP, // OLD 
  
  IPERF_IPC_MSG_RELAY_RESPOND, // IF relayer needs to do something instead of simply forwarding
  IPERF_IPC_MSG_RELAY_SERVICE_INTEREST, 

  IPERF_IPC_MSG_EXPECTATION_TIMEOUT,
  IPERF_IPC_MSG_INTEREST_TIMER_TIMEOUT,

  IPERF_IPC_MAX,
} IperfIPC_e;

typedef enum
{
  IPERF_MODE_TIMED,
  IPERF_MODE_SIZE,
  IPERF_MODE_CACHING_BIDIRECTIONAL,
  // TODO
  IPERF_MODE_MAX
} IperfMode_e;

typedef enum
{
  RECEIVER_STOPPED,
  RECEIVER_IDLE,
  RECEIVER_RECEIVING,
  RECEIVER_STATE_MAX
} IperfReceiverState_e;

typedef enum
{
  SENDER_STOPPED,
  SENDER_IDLE,
  SENDER_SENDING,
  SENDER_STATE_MAX
} IperfSenderState_e;

typedef enum
{
  IPERF_STATE_STOPPED,
  IPERF_STATE_IDLE,
  IPERF_STATE_SENDING,
  IPERF_STATE_WAITING_FOR_INTERESTS,
  IPERF_STATE_RECEIVING,
  IPERF_STATE_THREAD_STATE_MAX
} IperfThreadState_e;

typedef enum
{
  NOT_RECEIVED,
  REQUESTED,
  EXPECTED,
  RECEIVED,
  CHUNK_STATUS_MAX
} IperfChunkStatus_e;

typedef enum
{
  SENDER,
  RECEIVER,
  RELAYER,
  ROLE_MAX,
} IperfRole_e;

typedef struct 
{
  bool iAmSender; // If false we're the listener 
  
  uint16_t senderPort;
  uint16_t senderNetifId;
  uint16_t listenerPort;
  char *listenerIpStr;

  uint16_t payloadSizeBytes;
  uint16_t pktPerSecond;
  uint32_t delayUs;
  uint32_t interestDelayUs;
  uint32_t expectationDelayUs;

  IperfMode_e mode;
  IperfRole_e role;

  // TIMED MODE
  uint32_t transferTimeUs;

  // SIZE MODE
  uint32_t transferSizeBytes;

  uint16_t numPktsToTransfer;

  // Relay related
  bool cache;
  bool code;
  uint8_t numCacheBlocks;
  uint8_t cacheChancePercent;

} IperfConfig_s;

typedef struct
{
  int32_t lastPktSeqNo;
  uint32_t pktLossCounter;
  uint32_t numReceivedPkts;
  uint32_t numReceivedBytes;
  uint32_t numReceivedPayloadBytes;
  uint32_t numReceivedGoodBytes;
  uint32_t numDuplicates;
  uint32_t numSentPkts;
  uint32_t numSentBytes;
  uint32_t numInterestsSent;
  uint32_t numInterestsServed;
  uint16_t receivedUniqueChunks;
  uint32_t startTimestamp;
  uint32_t endTimestamp;

  uint16_t cacheHits;
  uint16_t cacheMisses;

  uint32_t l2numSentPackets;
  uint32_t l2numSentBytes;
  uint32_t l2numReceivedPackets;
  uint32_t l2numReceivedBytes;
  uint32_t l2numSuccessfulTx;
  uint32_t l2numErroredTx;

  uint32_t ipv6numSentPackets;
  uint32_t ipv6numSentBytes;
  uint32_t ipv6numReceivedPackets;
  uint32_t ipv6numReceivedBytes;
  uint32_t ipv6numSuccessfulTx;
  uint32_t ipv6numErroredTx;
} IperfResults_s;

static inline bool coinFlip(uint8_t percent)
{
  if (rand() % 100 < percent)
  {
    return true;
  }
  return false;
}

int Iperf_PacketHandler(gnrc_pktsnip_t *pkt, void (*fn) (gnrc_pktsnip_t *pkt));
int Iperf_StartUdpServer(gnrc_netreg_entry_t *server, kernel_pid_t pid);
int Iperf_StopUdpServer(gnrc_netreg_entry_t *server);
int Iperf_Deinit(void);
int Iperf_Init(IperfRole_e role);
void Iperf_ResetResults(void);
int Iperf_SocklessUdpSend(const char *data, size_t dataLen, ipv6_addr_t *addr, netif_t *netif);
int Iperf_SocklessUdpSendToStringAddr(const char *data, size_t dataLen, char *targetIp);
int Iperf_SocklessUdpSendToDst(const char *data, size_t dataLen);
int Iperf_SocklessUdpSendToSrc(const char *data, size_t dataLen);
void Iperf_PrintConfig(bool json);
void Iperf_PrintFileTransferStatus(void);
void Iperf_PrintFileContents(void);
void Iperf_HandleConfigSync(IperfUdpPkt_t *p);
int Iperf_SendInterest(uint16_t seqNo);
int Iperf_SendLastReceivedPacket(uint16_t lastSeqNo);
int Iperf_SendBulkInterest(uint16_t *interestArr, uint16_t len);
