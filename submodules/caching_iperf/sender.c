#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "thread.h"
#include "ztimer.h"
#include "msg.h"

#include "macros/utils.h"
#include "net/gnrc.h"
#include "net/sock/udp.h"
#include "net/gnrc/udp.h"
#include "net/sock/tcp.h"
#include "net/gnrc/nettype.h"
#include "net/utils.h"
#include "sema_inv.h"
#include "test_utils/benchmark_udp.h"
#include "ztimer.h"
#include "shell.h"
#include "od.h"
#include "net/netstats.h"

#include "iperf.h"
#include "iperf_pkt.h"
#include "message.h"
#include "logger.h"
#include "simple_queue.h"

#include "receiver.h"

extern IperfResults_s results;
extern IperfConfig_s config;
extern uint8_t rxtxBuffer[IPERF_BUFFER_SIZE_BYTES];
extern char dstGlobalIpAddr[25];
extern char srcGlobalIpAddr[25];
extern msg_t ipcMsg;
extern ztimer_t intervalTimer;
extern IperfThreadState_e iperfState;

extern uint16_t pktReqQueueBuffer[];
extern SimpleQueue_t pktReqQueue;

static uint8_t *txBuffer = (uint8_t *) &rxtxBuffer;
static msg_t _msg_queue[IPERF_MSG_QUEUE_SIZE];

static kernel_pid_t senderPid = KERNEL_PID_UNDEF;
static gnrc_netreg_entry_t udpServer = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL, KERNEL_PID_UNDEF);

#define TEST_FAKE_PACKET_DROP 0

static bool isTransferDone(void)
{
  bool ret = false;
  if (config.mode == IPERF_MODE_SIZE)
  {
    ret = (results.numSentBytes >= config.transferSizeBytes);
  }
  else if (config.mode == IPERF_MODE_TIMED)
  {
    ret = (ztimer_now(ZTIMER_USEC) - results.startTimestamp >= config.transferTimeUs);
  }
  else if (config.mode == IPERF_MODE_CACHING_BIDIRECTIONAL)
  {
    ret = (results.lastPktSeqNo == config.numPktsToTransfer-1) && (SimpleQueue_IsEmpty(&pktReqQueue));
  }
  return ret;
}

static int senderHandleIperfPacket(gnrc_pktsnip_t *pkt)
{
  IperfUdpPkt_t *iperfPkt = (IperfUdpPkt_t *) pkt;
  if (iperfPkt->msgType >= IPERF_CTRL_MSG_MAX)
  {
    logerror("Sender received bad msg_type %x\n", iperfPkt->msgType);
    return 1;
  }

  /*logverbose("Received seq no %d\nPayload %s\nLosses %d\nDups %d\n", iperfPkt->seqNo, iperfPkt->payload, results.pktLossCounter, results.numDuplicates);*/
  logverbose("Received Iperf Pkt: Type %d\n", iperfPkt->msgType);
  switch (iperfPkt->msgType)
  {
    case IPERF_PAYLOAD:
      {
        loginfo("Sender received PAYLOAD packet. Shouldnt have...\n");
        break;
      }
    case IPERF_PKT_REQ:
      {
        IperfInterest_t *reqPl = (IperfInterest_t *) iperfPkt->payload;
        loginfo("Sender received PKT_REQ for packet seq no %d\n", reqPl->seqNo);
        SimpleQueue_Push(&pktReqQueue, reqPl->seqNo);
        /*if (iperfState == IPERF_STATE_WAITING_FOR_INTERESTS)*/
        /*{*/
          ipcMsg.type = IPERF_IPC_MSG_SEND_FILE;
          ztimer_set_msg(ZTIMER_USEC, &intervalTimer, config.delayUs, &ipcMsg, senderPid); // Start immediately
        /*}*/
        break;
      }
    case IPERF_PKT_BULK_REQ:
      {
        IperfBulkInterest_t *bulkPl = (IperfBulkInterest_t *) iperfPkt->payload;
        logdebug("Sender received PKT_BULK_REQ for ");
        for (int i = 0; i < bulkPl->len; i++)
        {
          if (bulkPl->arr[i] == SIMPLE_QUEUE_INVALID_NUMBER)
          {
            continue;
          }
          SimpleQueue_Push(&pktReqQueue, bulkPl->arr[i]);
          if (logprintTags[DEBUG]) printf("%d ", bulkPl->arr[i]);
        }
        if (logprintTags[DEBUG]) printf("\n");
        if (!ztimer_is_set(ZTIMER_USEC, &intervalTimer))
        {
          ipcMsg.type = IPERF_IPC_MSG_SEND_FILE;
          ztimer_set_msg(ZTIMER_USEC, &intervalTimer, config.delayUs, &ipcMsg, senderPid); // Start immediately
        }
        break;
      }
    case IPERF_ECHO_CALL:
      {
        loginfo("Echo CALL Received %s\n", iperfPkt->payload);
        break;
      }
    case IPERF_ECHO_RESP:
      {
        loginfo("Echo RESP Received %s\n", iperfPkt->payload);
        break;
      }
    case IPERF_CONFIG_SYNC:
      {
        /*Iperf_HandleConfigSync(iperfPkt);*/
        break;
      }
    default:
      {
        logerror("Bad iperf packet type %d\n", iperfPkt->msgType);
        break;
      }
  }
  return 0;
}

static void initSender(void)
{
  senderPid = thread_getpid();
  memset(txBuffer, 0x01, sizeof(IperfUdpPkt_t) + config.payloadSizeBytes);
  IperfUdpPkt_t *payloadPkt = (IperfUdpPkt_t *) txBuffer;
  payloadPkt->plSize = config.payloadSizeBytes;
  payloadPkt->msgType = IPERF_PAYLOAD;
  payloadPkt->seqNo = 0;
  strncpy((char *) &payloadPkt->payload, IperfMessage_GetPointer(0), config.payloadSizeBytes);
  Iperf_StartUdpServer(&udpServer, senderPid);
}

static void deinitSender(void)
{
  Iperf_StopUdpServer(&udpServer);
}

static int sendPayload(uint16_t chunkIdx, bool serveRequest)
{
  IperfUdpPkt_t *payloadPkt = (IperfUdpPkt_t *) txBuffer;
  uint16_t charIdx = chunkIdx * config.payloadSizeBytes;
  strncpy((char *) &payloadPkt->payload, IperfMessage_GetPointer(charIdx), config.payloadSizeBytes);
  logverbose("Sending payload. Seq no %d\n", payloadPkt->seqNo);
  return Iperf_SocklessUdpSendToDst((char *) txBuffer, config.payloadSizeBytes + sizeof(IperfUdpPkt_t));
}

static void handleFileSending(void)
{
  IperfUdpPkt_t *payloadPkt = (IperfUdpPkt_t *) txBuffer;

  if (SimpleQueue_IsEmpty(&pktReqQueue)) // If nothing in the request queue, send off the next chunk in our file transfer
  {
    payloadPkt->msgType = IPERF_PAYLOAD;
    payloadPkt->seqNo = results.lastPktSeqNo + 1;
    payloadPkt->plSize = config.payloadSizeBytes;
    uint16_t charIdx = payloadPkt->seqNo * config.payloadSizeBytes;
    strncpy((char *) &payloadPkt->payload, IperfMessage_GetPointer(charIdx), config.payloadSizeBytes);
    logverbose("Sending payload. Seq no %d\n", payloadPkt->seqNo);
    results.lastPktSeqNo = payloadPkt->seqNo;

    #if TEST_FAKE_PACKET_DROP
    if ((results.lastPktSeqNo > 10 && results.lastPktSeqNo < 25))
    {
      loginfo("TEST_FAKE_PACKET_DROP FAKE DROPPING PACKET %d\n", results.lastPktSeqNo);
    }
    else 
    {
      Iperf_SocklessUdpSendToDst((char *) txBuffer, config.payloadSizeBytes + sizeof(IperfUdpPkt_t));
    }
    #else
    Iperf_SocklessUdpSendToDst((char *) txBuffer, config.payloadSizeBytes + sizeof(IperfUdpPkt_t));
    #endif
  }
  else // If there's something in the req queue, send that now
  {
    uint16_t reqSeq = 0;
    int ret = SimpleQueue_Pop(&pktReqQueue, &reqSeq);
    payloadPkt->msgType = IPERF_PKT_RESP;
    payloadPkt->seqNo = reqSeq;
    payloadPkt->plSize = config.payloadSizeBytes;
    results.numInterestsServed++;
    uint16_t charIdx = payloadPkt->seqNo * config.payloadSizeBytes;
    strncpy((char *) &payloadPkt->payload, IperfMessage_GetPointer(charIdx), config.payloadSizeBytes);
    logdebug("Serving request. Seq no %d\n", payloadPkt->seqNo);
    Iperf_SocklessUdpSendToDst((char *) txBuffer, config.payloadSizeBytes + sizeof(IperfUdpPkt_t));
  }

  /*// TODO kinda ugly. this is so that if we got an interest pkt thru a user cmd, we dont keep running the timer*/
  if (iperfState == IPERF_STATE_IDLE)
  {
    if (!SimpleQueue_IsEmpty(&pktReqQueue))
    {
      ztimer_set_msg(ZTIMER_USEC, &intervalTimer, config.delayUs, &ipcMsg, senderPid);
    }
    return;
  }

  results.numSentBytes += config.payloadSizeBytes;// + sizeof(IperfUdpPkt_t); // todo should or should not include metadata in our size sum? (4 bytes)
  if (isTransferDone())
  {
    loginfo("Sender done sending %d packets\n", config.numPktsToTransfer);
    if (config.mode < IPERF_MODE_CACHING_BIDIRECTIONAL)
    {
      iperfState = IPERF_STATE_STOPPED;
      results.endTimestamp = ztimer_now(ZTIMER_USEC);
      loginfo("Stopping iperf\n");
    }
    else if (config.mode == IPERF_MODE_CACHING_BIDIRECTIONAL)
    {
      iperfState = IPERF_STATE_WAITING_FOR_INTERESTS;
      loginfo("Sitting idle\n");
    }
  }
  else
  {
    /*payloadPkt->seqNo++;*/
    ztimer_set_msg(ZTIMER_USEC, &intervalTimer, config.delayUs, &ipcMsg, senderPid);
  }
}

void *Iperf_SenderThread(void *arg)
{
  (void) arg;
  msg_t msg, reply;
  msg_init_queue(_msg_queue, IPERF_MSG_QUEUE_SIZE);

  reply.content.value = (uint32_t)(-ENOTSUP);
  reply.type = GNRC_NETAPI_MSG_TYPE_ACK;

  initSender();
  loginfo("Starting Sender Thread. Sitting Idle. Pid %d\n", senderPid);

  iperfState = IPERF_STATE_IDLE;

  ipcMsg.type = IPERF_IPC_MSG_SEND_FILE;

  do {
    msg_receive(&msg);
    logdebug("IPC Message type %x\n", msg.type);
    switch (msg.type) {
      case GNRC_NETAPI_MSG_TYPE_RCV:
        {
          logdebug("Data received\n");
          Iperf_PacketHandler(msg.content.ptr, (void *) senderHandleIperfPacket);
          break;
        }
      case GNRC_NETAPI_MSG_TYPE_SND:
        {
          logdebug("Data to send\n");
          /*Iperf_PacketHandler(msg.content.ptr, senderHandleIperfPacket);*/
          break;
        }
      case GNRC_NETAPI_MSG_TYPE_GET:
      case GNRC_NETAPI_MSG_TYPE_SET:
        {
          msg_reply(&msg, &reply);
          break;
        }
      case IPERF_IPC_MSG_START: // TODO am i adding complexity for no reason? 
        {
          loginfo("Sender received START command. Commencing iperf\n");
          iperfState = IPERF_STATE_SENDING;
          Iperf_ResetResults();
          results.startTimestamp = ztimer_now(ZTIMER_USEC);
          ztimer_set_msg(ZTIMER_USEC, &intervalTimer, 0, &ipcMsg, senderPid); // Start immediately
          break;
        }
      case IPERF_IPC_MSG_SEND_FILE:
        {
          handleFileSending();
          break;
        }
      case IPERF_IPC_MSG_IDLE:
        {
          results.endTimestamp = ztimer_now(ZTIMER_USEC);
          iperfState = IPERF_STATE_IDLE;
        }
      case IPERF_IPC_MSG_STOP:
        {
          results.endTimestamp = ztimer_now(ZTIMER_USEC);
          iperfState = IPERF_STATE_STOPPED;
          break;
        }
      default:
        logverbose("received something unexpected");
        break;
    }
  } while (iperfState > IPERF_STATE_STOPPED);
  deinitSender();
  loginfo("Sender thread exiting\n");
  return NULL;
}
