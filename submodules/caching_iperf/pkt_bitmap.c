#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "iperf.h"
#include "iperf_pkt.h"

// Lets use globals. quick and a little dirty 

/*typedef struct {*/
/*  uint16_t pktOffset; // 2*/
/*  uint8_t len; // 1 // todo do we want this? */
/*  uint8_t bitmap[];*/
/*} __attribute__((packed)) IperfReceivedPktBitmap_t;*/

// TODO find better name for this scheme
void PktBitmap_PopulatePktBitmap(IperfUdpPkt_t *iperfPkt, IperfChunkStatus_e *receivedPktIds, uint16_t offset, uint8_t len)
{
  IperfReceivedPktBitmap_t *bmp = (IperfReceivedPktBitmap_t *) iperfPkt->payload;
  bmp->pktOffset = offset;
  bmp->len = len;

  // Fill in the bitmap
  // Received Pkt Ids v
  // [            |       |    ]
  //        offset^    len^
  
}
