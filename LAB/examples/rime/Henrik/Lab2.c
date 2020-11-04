

/*  HENRIK BRÅDLAND */

#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"

#include "dev/i2cmaster.h"
#include "dev/tmp102.h"

#include <stdio.h>



#define TMP102_READ_INTERVAL (CLOCK_SECOND/2)

struct tempData{
      char minus;
      int16_t tempint;
      uint16_t tempfrac;
};

struct tempData td;


/* We first declare our three processes. */
PROCESS(broadcast_process, "Broadcast process");
PROCESS(unicast_process, "Unicast process");
PROCESS(temp_process, "Test Temperature process");
//AUTOSTART_PROCESSES(&temp_process);
/*---------------------------------------------------------------------------*/
static struct etimer et;

PROCESS_THREAD(temp_process, ev, data)
{
  PROCESS_BEGIN();
  printf("TEMP_PROCESS BEGIN \n");

  int16_t tempint;
  uint16_t tempfrac;
  int16_t raw;
  uint16_t absraw;
  int16_t sign;
  char minus = ' ';

  tmp102_init();

  while(1) {
    etimer_set(&et, TMP102_READ_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    sign = 1;

    PRINTFDEBUG("Reading Temp...\n");
    raw = tmp102_read_temp_raw();
    absraw = raw;
    if(raw < 0) {		// Perform 2C's if sensor returned negative data
      absraw = (raw ^ 0xFFFF) + 1;
      sign = -1;
    }
    tempint = (absraw >> 8) * sign;
    tempfrac = ((absraw >> 4) % 16) * 625;	// Info in 1/10000 of degree
    minus = ((tempint == 0) & (sign == -1)) ? '-' : ' ';
    td.minus = minus;
    td.tempfrac = tempfrac;
    td.tempint = tempint;
    printf("Temp = %c%d.%04d\n", minus, tempint, tempfrac);
  }


  AUTOSTART_PROCESSES(&unicast_process);
  PROCESS_END();
}



/* This is the structure of broadcast messages. */
struct broadcast_message {
  uint8_t seqno;
};

/* This is the structure of unicast ping messages. */
struct unicast_message {
  uint8_t type;
  struct tempData td;
};

/* These are the types of unicast messages that we can send. */
enum {
  UNICAST_TYPE_PING,
  UNICAST_TYPE_PONG
};
/* This #define defines the maximum amount of neighbors we can remember. */
#define MAX_NEIGHBORS 16
#define MAX_INFO_PER_NEIGHBOR 10





/* This structure holds information about neighbors. */
struct neighbor {
  /* The ->next pointer is needed since we are placing these on a
     Contiki list. */
  struct neighbor *next;

  /* The ->addr field holds the Rime address of the neighbor. */
  linkaddr_t addr;

  /* The ->last_rssi and ->last_lqi fields hold the Received Signal
     Strength Indicator (RSSI) and CC2420 Link Quality Indicator (LQI)
     values that are received for the incoming broadcast packets. */
  uint16_t last_rssi, last_lqi;

  /* Each broadcast packet contains a sequence number (seqno). The
     ->last_seqno field holds the last sequenuce number we saw from
     this neighbor. */
  uint8_t last_seqno;

  /* The ->avg_gap contains the average seqno gap that we have seen
     from this neighbor. */
  uint32_t avg_seqno_gap;
  
  struct tempData data[MAX_INFO_PER_NEIGHBOR];

};

static struct neighbor my_neighbors[MAX_NEIGHBORS] = { 0 };
uint8_t numbeOfNeighbors = 0;



/* This MEMB() definition defines a memory pool from which we allocate
   neighbor entries. */
//MEMB(neighbors_memb, struct neighbor, MAX_NEIGHBORS);

/* The neighbors_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(neighbors_list);

/* These hold the broadcast and unicast structures, respectively. */
static struct broadcast_conn broadcast;
static struct unicast_conn unicast;

uint16_t updateLQI(uint16_t oldData, uint16_t newData){
  printf("LQI is updated \n");
  return (oldData * 7 + newData * 3)/10;
}


/* These two defines are used for computing the moving average for the
   broadcast sequence number gaps. */
#define SEQNO_EWMA_UNITY 0x100
#define SEQNO_EWMA_ALPHA 0x040

/*---------------------------------------------------------------------------*/

/* The AUTOSTART_PROCESSES() definition specifices what processes to
   start when this module is loaded. We put both our processes
   there. */
AUTOSTART_PROCESSES(&broadcast_process, &unicast_process, &temp_process);
/*---------------------------------------------------------------------------*/
/* This function is called whenever a broadcast message is received. */
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct neighbor *n;
  struct broadcast_message *m;
  uint8_t seqno_gap;

  /* The packetbuf_dataptr() returns a pointer to the first data byte
     in the received packet. */
  m = packetbuf_dataptr();

  /* Print out a message. */
  printf("broadcast message received from %d.%d with seqno %d, RSSI %u, LQI %u, avg seqno gap %d.%02d\n",
         from->u8[0], from->u8[1],
         m->seqno,
         packetbuf_attr(PACKETBUF_ATTR_RSSI),
         packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY),
         (int)(n->avg_seqno_gap / SEQNO_EWMA_UNITY),
         (int)(((100UL * n->avg_seqno_gap) / SEQNO_EWMA_UNITY) % 100));


  //static uint8_t my_neighbors[MAX_NEIGHBORS][MAX_INFO_PER_NEIGHBOR] = { 0 };
  //uint8_t numbeOfNeighbors = 0;




  //Updating the data from the neighbors
  uint8_t myFlag = 0;
  uint8_t ii = 0;
  uint8_t jj = 0;

  for(ii = 0; ii < numbeOfNeighbors; ii++){
      if (linkaddr_cmp(from, &my_neighbors[ii].addr)){
          my_neighbors[ii].last_lqi = updateLQI(my_neighbors[ii].last_lqi, packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
          myFlag = 1;
          break;
      }
  }


  // If the broadcast comes from a new node, it will now be stored
  if (myFlag == 0 && numbeOfNeighbors < MAX_NEIGHBORS-1){
      linkaddr_copy(&my_neighbors[numbeOfNeighbors].addr, from);
      //my_neighbors[numbeOfNeighbors].addr.u8[0] = from->u8[0];
      //my_neighbors[numbeOfNeighbors].addr.u8[1] = from->u8[1];
      my_neighbors[numbeOfNeighbors].last_lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
      numbeOfNeighbors++;
      printf("New neighbor!! The total is now %i\n", numbeOfNeighbors);
  }



}

/* This is where we define what function to be called when a broadcast
   is received. We pass a pointer to this structure in the
   broadcast_open() call below. */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  struct unicast_message *msg;

  /* Grab the pointer to the incoming data. */
  msg = packetbuf_dataptr();
  uint8_t ii, jj;

  /* We have two message types, UNICAST_TYPE_PING and
     UNICAST_TYPE_PONG. If we receive a UNICAST_TYPE_PING message, we
     print out a message and return a UNICAST_TYPE_PONG. */
  if(msg->type == UNICAST_TYPE_PING) {
    printf("unicast ping received from %d.%d\n Received temp = %c%d.%04d\n",
           from->u8[0], from->u8[1], msg->td.minus, msg->td.tempint, msg->td.tempfrac);
    
    for(ii = 0; ii < numbeOfNeighbors; ii++){
        if (linkaddr_cmp(&my_neighbors[ii].addr, from)){
            for(jj = MAX_INFO_PER_NEIGHBOR; jj > 0; jj--){
                my_neighbors[ii].data[jj] = my_neighbors[ii].data[jj-1];
                my_neighbors[ii].data[0].minus = my_neighbors[ii].data[0].minus;
                my_neighbors[ii].data[0].tempfrac = my_neighbors[ii].data[0].tempfrac;
                my_neighbors[ii].data[0].tempint = my_neighbors[ii].data[0].tempint;
            }
            my_neighbors[ii].data[0].minus = msg->td.minus;
            my_neighbors[ii].data[0].tempfrac = msg->td.tempfrac;
            my_neighbors[ii].data[0].tempint = msg->td.tempint;
            
        }
    }
    
    
    msg->type = UNICAST_TYPE_PONG;
    msg->td.minus = 0;
    msg->td.tempfrac = 0;    
    msg->td.tempint = 0;
    packetbuf_copyfrom(msg, sizeof(struct unicast_message));
    /* Send it back to where it came from. */
    unicast_send(c, from);
  }
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;
  static uint8_t seqno;
  struct broadcast_message msg;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();
    printf("BROADCAST_PROCESS BEGIN \n");

  broadcast_open(&broadcast, 129, &broadcast_call);

      printf("hei\n");

  while(1) {

    /* Send a broadcast every 16 - 32 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    msg.seqno = seqno;
    packetbuf_copyfrom(&msg, sizeof(struct broadcast_message));
    broadcast_send(&broadcast);
    seqno++;
    printf("New broadcast sendt \n");
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&unicast);)
    
  PROCESS_BEGIN();
    printf("UNICAST_PROCESS BEGIN \n");

  unicast_open(&unicast, 146, &unicast_callbacks);

  while(1) {
    static struct etimer et;
    struct unicast_message msg;
    struct neighbor *n;
    uint16_t tempLQI = 0;
    uint8_t ii;

    etimer_set(&et, CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 2));
    


    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    msg.td.minus = td.minus;
    msg.td.tempfrac = td.tempfrac;
    msg.td.tempint = td.tempint;


    for(ii = 0; ii < numbeOfNeighbors; ii++){
        if (my_neighbors[ii].last_lqi > tempLQI){
            tempLQI = my_neighbors[ii].last_lqi;
            linkaddr_copy(&n->addr, &my_neighbors[ii].addr);
        }
    }
    msg.type = UNICAST_TYPE_PING;
    packetbuf_copyfrom(&msg, sizeof(msg));
    unicast_send(&unicast, &n->addr);
    printf("New unicast sendt to %d.%d\n", n->addr.u8[0], n->addr.u8[1]);

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
