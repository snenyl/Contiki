/*
IKT435 - Lab 2 Nylund



*/

//Temperature includes
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#include "dev/leds.h"

#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"

#include <stdio.h>
#include <stdbool.h>

//#include<stdbool.h> //Needed for sorting algorthm.

/* This is the structure of broadcast messages. */
struct broadcast_message {
  uint8_t seqno;
};

/* This is the structure of unicast ping messages. */
struct unicast_message {
  uint8_t type;
};

/* These are the types of unicast messages that we can send. */
enum {
  UNICAST_TYPE_PING,
  UNICAST_TYPE_PONG
};

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

};

/*Temperature struct*/
struct temperature{
	int16_t tempint;
	uint16_t tempfrac;
	char minus;
};


/* This #define defines the maximum amount of neighbors we can remember. */
#define MAX_NEIGHBORS 16


static struct neighbor my_neighbors[MAX_NEIGHBORS] = { 0 };
uint8_t totalDevicesNearby = 0;


/* This MEMB() definition defines a memory pool from which we allocate
   neighbor entries. */
MEMB(neighbors_memb, struct neighbor, MAX_NEIGHBORS);

/* The neighbors_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(neighbors_list);

/* These hold the broadcast and unicast structures, respectively. */
static struct broadcast_conn broadcast;
static struct unicast_conn unicast;

/* These two defines are used for computing the moving average for the
   broadcast sequence number gaps. */
#define SEQNO_EWMA_UNITY 0x100
#define SEQNO_EWMA_ALPHA 0x040


/*
Defenitions for temperature process.
*/
#define TMP102_READ_INTERVAL (CLOCK_SECOND*2	)

//Array for saving nearby nodes:
int16_t localNeightbors[]={4,2};

uint8_t *globalAdress0ptr;
uint8_t *globalAdress1ptr;












/*---------------------------------------------------------------------------*/
/* We first declare our two processes. */
PROCESS(broadcast_process, "Broadcast process");
PROCESS(unicast_process, "Unicast process");

/* The AUTOSTART_PROCESSES() definition specifices what processes to
   start when this module is loaded. We put both our processes
   there. */
AUTOSTART_PROCESSES(&broadcast_process, &unicast_process);
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

  /* Check if we already know this neighbor. */
  for(n = list_head(neighbors_list); n != NULL; n = list_item_next(n)) {

    /* We break out of the loop if the address of the neighbor matches
       the address of the neighbor from which we received this
       broadcast message. */
    if(linkaddr_cmp(&n->addr, from)) {
      break;
    }
  }

  /* If n is NULL, this neighbor was not found in our list, and we
     allocate a new struct neighbor from the neighbors_memb memory
     pool. */
  if(n == NULL) {
    n = memb_alloc(&neighbors_memb);

    /* If we could not allocate a new neighbor entry, we give up. We
       could have reused an old neighbor entry, but we do not do this
       for now. */
    if(n == NULL) {
      return;
    }

    /* Initialize the fields. */
    linkaddr_copy(&n->addr, from);
    n->last_seqno = m->seqno - 1;
    n->avg_seqno_gap = SEQNO_EWMA_UNITY;

    /* Place the neighbor on the neighbor list. */
    list_add(neighbors_list, n);
  }

  /* We can now fill in the fields in our neighbor entry. */
  n->last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  n->last_lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);

  /* Compute the average sequence number gap we have seen from this neighbor. */
  seqno_gap = m->seqno - n->last_seqno;
  n->avg_seqno_gap = (((uint32_t)seqno_gap * SEQNO_EWMA_UNITY) *
                      SEQNO_EWMA_ALPHA) / SEQNO_EWMA_UNITY +
                      ((uint32_t)n->avg_seqno_gap * (SEQNO_EWMA_UNITY -
                                                     SEQNO_EWMA_ALPHA)) /
    SEQNO_EWMA_UNITY;

  /* Remember last seqno we heard. */
  n->last_seqno = m->seqno;

  /* Print out a message. */
  printf("broadcast message received from %d.%d with seqno %d, RSSI %u, LQI %u, avg seqno gap %d.%02d\n",
         from->u8[0], from->u8[1],
         m->seqno,
         packetbuf_attr(PACKETBUF_ATTR_RSSI),
         packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY),
         (int)(n->avg_seqno_gap / SEQNO_EWMA_UNITY),
         (int)(((100UL * n->avg_seqno_gap) / SEQNO_EWMA_UNITY) % 100));


  printf("This is the last link Quality: %u \n",
        packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));

  if (1) {
    /* code */
  }
  //localNeightbors


  // printf("First neighbor %u \n",
  //       localNeightbors[0]);
  //
  // printf("Second neighbor %u \n",
  //       localNeightbors[1]);

  //localNeightbors[0]

  uint8_t myFlag = 0;
  uint8_t ii = 0;
  uint8_t iii = 0;
//  uint8_t jj = 0;

  for(ii = 0; ii < totalDevicesNearby; ii++){
      if (linkaddr_cmp(from, &my_neighbors[ii].addr))
      {
  //        my_neighbors[ii].last_lqi = updateLQI(my_neighbors[ii].last_lqi, packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
          myFlag = 1;
          break;
      }
  }




  // If the broadcast comes from a new node, it will now be stored
  if (myFlag == 0 && totalDevicesNearby < MAX_NEIGHBORS-1){
      linkaddr_copy(&my_neighbors[totalDevicesNearby].addr, from);
      my_neighbors[totalDevicesNearby].addr.u8[0] = from->u8[0];
      my_neighbors[totalDevicesNearby].addr.u8[1] = from->u8[1];
      my_neighbors[totalDevicesNearby].last_lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
      totalDevicesNearby++;
      printf("--------------- New friend was located! --------------- \n");
      printf("Total neighbors: %i\n", totalDevicesNearby);
  }


  //function for sorting based of LQI
  uint8_t arraySize = 3; //totalDevicesNearby
  uint8_t sortOutput0 = 0;
  uint8_t sortOutput1 = 0;


  for(iii = 0; iii < arraySize; iii++){
    sortOutput0 = my_neighbors[iii].addr.u8[0];
    sortOutput1 = my_neighbors[iii].addr.u8[1];
    printf("Addresses: %i.%i\n", sortOutput0,sortOutput1);
  }

//Print the varables
printf("First frend LQI: %i with adress %i.%i\n", my_neighbors[0].last_lqi, my_neighbors[0].addr.u8[0], my_neighbors[0].addr.u8[1]);
printf("Second frend LQI: %i with adress %i.%i\n", my_neighbors[1].last_lqi, my_neighbors[1].addr.u8[0], my_neighbors[1].addr.u8[1]);
printf("Third frend LQI: %i with adress %i.%i\n", my_neighbors[2].last_lqi, my_neighbors[2].addr.u8[0], my_neighbors[2].addr.u8[1]);


//Variables for sort (Type: Bubblesort)
int iiii, j, tempSortLQI, tempSortAddress0, tempSortAddress1;
bool swapped;

for(iiii = 0; iiii < totalDevicesNearby - 1; iiii++) // to keep track of number of iteration
  {
  swapped = false; // to check whether swapping of two elements happened or not
  for(j = 0; j < totalDevicesNearby - iiii - 1; j++) // to compare the elements within the particular iteration
    {

    // swap if any element is greater than its adjacent element
    if(my_neighbors[j].last_lqi > my_neighbors[j + 1].last_lqi)
      {
        tempSortLQI = my_neighbors[j].last_lqi;
        tempSortAddress0 = my_neighbors[j].addr.u8[0];
        tempSortAddress1 = my_neighbors[j].addr.u8[1];
        my_neighbors[j] = my_neighbors[j + 1]; //Moves the one above
        my_neighbors[j + 1].last_lqi = tempSortLQI;
        my_neighbors[j + 1].addr.u8[0] = tempSortAddress0;
        my_neighbors[j + 1].addr.u8[1] = tempSortAddress1;
        swapped = true;
      }

    }

  // if swapping of two elements does not happen then break
  if(swapped == false)
  break;
  }
















}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  struct unicast_message *msg;

  struct temperature t;
  memcpy(&t, (struct temperature *)packetbuf_dataptr(), sizeof(t));
  printf("unicast message received from %d.%d: %c%d.%04d\n",
   from->u8[0], from->u8[1], t.minus, t.tempint, t.tempfrac);


  // /* Grab the pointer to the incoming data. */
  // msg = packetbuf_dataptr();
  //
  // /* We have two message types, UNICAST_TYPE_PING and
  //    UNICAST_TYPE_PONG. If we receive a UNICAST_TYPE_PING message, we
  //    print out a message and return a UNICAST_TYPE_PONG. */
  // if(msg->type == UNICAST_TYPE_PING) {
  //   printf("unicast ping received from %d.%d\n",
  //          from->u8[0], from->u8[1]);
  //   msg->type = UNICAST_TYPE_PONG;
  //   packetbuf_copyfrom(msg, sizeof(struct unicast_message));
  //   /* Send it back to where it came from. */
  //   unicast_send(c, from);
  // }
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;
  static uint8_t seqno;
  struct broadcast_message msg;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);


  while(1) {

    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("Hello", 6);
    broadcast_send(&broadcast);
    printf("broadcast message sent\n");
  }

  // while(1) {
  //
  //   /* Send a broadcast every 3 - 4 seconds */
  //   etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4)); //Default was 16 16
  //
  //   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  //
  //   msg.seqno = seqno;
  //   packetbuf_copyfrom(&msg, sizeof(struct broadcast_message));
  //   broadcast_send(&broadcast);
  //   seqno++;
  // }

  PROCESS_END();
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
//static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data) //UNICAST SEND!
{
  static struct etimer et;
//  PROCESS_EXITHANDLER(unicast_close(&uc);)
  PROCESS_EXITHANDLER(unicast_close(&unicast);)

  PROCESS_BEGIN();

  unicast_open(&unicast, 146, &unicast_callbacks);
  tmp102_init();


  while(1) {
    static struct etimer et;
    struct unicast_message msg;
    struct neighbor *n;
    int randneighbor, i;

    int16_t raw;
    uint16_t absraw;
    int16_t sign;
    struct temperature t;


    //etimer_set(&et, CLOCK_SECOND * 8 + random_rand() % (CLOCK_SECOND * 8));
    etimer_set(&et, TMP102_READ_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
//    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // /* Pick a random neighbor from our list and send a unicast message to it. */
    // if(list_length(neighbors_list) > 0) {
    //   randneighbor = random_rand() % list_length(neighbors_list);
    //   n = list_head(neighbors_list);
    //   for(i = 0; i < randneighbor; i++) {
    //     n = list_item_next(n);
    //   }

    /* Pick the neighbor with lowest LQI from our list and send a unicast message to it. */

    linkaddr_t uniOutAdress;


    if(list_length(neighbors_list) > 0) {
     uniOutAdress.u8[0] = my_neighbors[0].addr.u8[0];
     uniOutAdress.u8[1] = my_neighbors[0].addr.u8[1];

     // uniOutAdress.addr.u8[0] = my_neighbors[0].addr.u8[0];
     // uniOutAdress.addr.u8[1] = my_neighbors[0].addr.u8[1];

       printf("Running UNICAST PROCESS \n");

      // randneighbor = random_rand() % list_length(neighbors_list);
      // n = list_head(neighbors_list);

      // for(i = 0; i < randneighbor; i++)

      //   n = list_item_next(n);

      //printf("sending unicast to %d.%d\n", uniOutAdress.u8[0], uniOutAdress.u8[1]);

      globalAdress0ptr = uniOutAdress.u8[0];
      globalAdress1ptr = uniOutAdress.u8[1];

    //  printf("Global address in UNICAST: %d and %d\n", globalAdress0ptr, &globalAdress0ptr);

      // msg.type = UNICAST_TYPE_PING;
      // packetbuf_copyfrom(&msg, sizeof(msg));
      // unicast_send(&unicast, &uniOutAdress);
      /*
      TEMPERATURE
      */



        sign = 1;
        linkaddr_t addr;

        PRINTFDEBUG("Reading Temp...\n");
        raw = tmp102_read_temp_raw();
        absraw = raw;
        if(raw < 0)
          {		// Perform 2C's if sensor returned negative data
            absraw = (raw ^ 0xFFFF) + 1;
            sign = -1;
          }
        t.tempint = (absraw >> 8) * sign;
        t.tempfrac = ((absraw >> 4) % 16) * 625;	// Info in 1/10000 of degree
        t.minus = ((t.tempint == 0) & (sign == -1)) ? '-' : ' ';
        packetbuf_copyfrom(&t, sizeof(t));

        addr.u8[0] = globalAdress0ptr; // earlier it was 238
        addr.u8[1] = globalAdress1ptr;

        //  printf("Global address in TEMPERATURE: %d and %d\n",globalAdress0ptr, &globalAdress0ptr);

          if(!linkaddr_cmp(&addr, &linkaddr_node_addr))
            {
              unicast_send(&unicast, &addr);
              printf("Unicast message sent to: %d.%d  temp: %c%d.%04d\n",addr.u8[0], addr.u8[1], t.minus, t.tempint, t.tempfrac);
            }

      }

  }

  PROCESS_END();
}


/*---------------------------------------------------------------------------*/
// PROCESS_THREAD(temp_process, ev, data)
// {
//   static struct etimer et;
//   PROCESS_EXITHANDLER(unicast_close(&uc);)
//
//   PROCESS_BEGIN();
//
//   int16_t raw;
//   uint16_t absraw;
//   int16_t sign;
//   struct temperature t;
//
//   tmp102_init();
//   unicast_open(&uc, 146, &unicast_callbacks);
//
//   while(1) {
//     etimer_set(&et, TMP102_READ_INTERVAL);
//     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
//
//     sign = 1;
//     linkaddr_t addr;
//
//     PRINTFDEBUG("Reading Temp...\n");
//     raw = tmp102_read_temp_raw();
//     absraw = raw;
//     if(raw < 0) {		// Perform 2C's if sensor returned negative data
//       absraw = (raw ^ 0xFFFF) + 1;
//       sign = -1;
//     }
//     t.tempint = (absraw >> 8) * sign;
//     t.tempfrac = ((absraw >> 4) % 16) * 625;	// Info in 1/10000 of degree
//     t.minus = ((t.tempint == 0) & (sign == -1)) ? '-' : ' ';
//     packetbuf_copyfrom(&t, sizeof(t));
//
//     addr.u8[0] = globalAdress0ptr; // earlier it was 238
//     addr.u8[1] = globalAdress1ptr;
//
//   //  printf("Global address in TEMPERATURE: %d and %d\n",globalAdress0ptr, &globalAdress0ptr);
//
//     if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
//       unicast_send(&uc, &addr);
//       printf("Unicast message sent to: %d.%d  temp: %c%d.%04d\n",addr.u8[0], addr.u8[1], t.minus, t.tempint, t.tempfrac);
//     }
//
//   }
//   PROCESS_END();
// }

/*---------------------------------------------------------------------------*/
