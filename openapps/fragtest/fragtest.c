#include "opendefs.h"
#include "fragtest.h"
#include "openudp.h"
#include "openqueue.h"
#include "opentimers.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "IEEE802154E.h"
#include "idmanager.h"
#include "openrandom.h"
#include "fragment.h"

//=========================== variables =======================================

fragtest_vars_t fragtest_vars;

static const uint8_t fragtest_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
}; 
/*
static const uint8_t fragtest_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x14, 0x15, 0x92, 0xcc, 0x00, 0x00, 0x00, 0x02
};
static const uint8_t fragtest_org_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x14, 0x15, 0x92, 0xcc, 0x00, 0x00, 0x00, 0x04
};
*/
//=========================== prototypes ======================================

void fragtest_timer_cb(opentimer_id_t id);
void fragtest_task_cb(void);

//=========================== public ==========================================

void fragtest_init() {
   
   // clear local variables
   memset(&fragtest_vars,0,sizeof(fragtest_vars_t));
   
   // start periodic timer
   fragtest_vars.timerId = opentimers_start(
      FRAGTEST_PERIOD_MS,
      TIMER_PERIODIC,TIME_MS,
      fragtest_timer_cb
   );
}

void fragtest_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   if ( error == E_FAIL )
      printf("FRAGTEST: Error on transmission\n");
   openqueue_freePacketBuffer(msg);
}

void fragtest_receive(OpenQueueEntry_t* pkt) {
   uint16_t i;
   uint16_t aux;

   for ( i = 0; i < pkt->length; i++ )
      if (pkt->payload[i] != i % 10 )
	 break;
   printf("FRAGTEST: Incoming message - %dB read from %d\n", i, pkt->length);
   aux = pkt->length;
   openqueue_freePacketBuffer(pkt);
   
   openserial_printError(
      COMPONENT_FRAGTEST,
      ERR_RCVD_ECHO_REPLY,
      (errorparameter_t)aux,
      (errorparameter_t)i
   );
}

//=========================== private =========================================

/**
\note timer fired, but we don't want to execute task in ISR mode instead, push
   task to scheduler with CoAP priority, and let scheduler take care of it.
*/
void fragtest_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(fragtest_task_cb,TASKPRIO_COAP);
}

void fragtest_task_cb() {
   OpenQueueEntry_t*    pkt;
   uint16_t             counter;
   uint16_t             i;
//   open_addr_t          origin;
   
   // don't run if not synch
   if (ieee154e_isSynch() == FALSE) return;
   
   // don't run on dagroot
   if (idmanager_getIsDAGroot()) {
      opentimers_stop(fragtest_vars.timerId);
      return;
   }
   
   // if you get here, send a packet
   
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(COMPONENT_FRAGTEST);
   if (pkt==NULL) {
      openserial_printError(
         COMPONENT_FRAGTEST,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   pkt->owner                         = COMPONENT_FRAGTEST;
   pkt->creator                       = COMPONENT_FRAGTEST;
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_FRAGTEST;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_FRAGTEST;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],fragtest_dst_addr,16);

/*   origin.type = ADDR_128B;
   memcpy(&origin.addr_128b[0], fragtest_org_addr, 16);
   
   // Don't run on destination
*   if ( idmanager_isMyAddress(&(pkt->l3_destinationAdd)) ) {
      printf("FRAG: My ID (%lu)\n", (unsigned long int)self);
      openqueue_freePacketBuffer(pkt);
      opentimers_stop(fragtest_vars.timerId);
      return;
   }/
   // Just run on origin
   if ( ! idmanager_isMyAddress(&origin) ) {
      openqueue_freePacketBuffer(pkt);
      opentimers_stop(fragtest_vars.timerId);
      return;
   }*/

   counter = openrandom_get16b() % (FRAGMENT_MAX_SIZE-125);
   packetfunctions_reserveHeaderSize(pkt,counter * sizeof(uint8_t));
   for ( i = 0; i < counter; i++ )
      pkt->payload[i] = i % 10;
   
   printf("FRAGTEST: Sending %d octets\n", counter);
   if ((openudp_send(pkt))==E_FAIL)
      openqueue_freePacketBuffer(pkt);
}
