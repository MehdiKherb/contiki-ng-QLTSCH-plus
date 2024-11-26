/********** Libraries ***********/
#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "sys/node-id.h"
#include <inttypes.h>
// #include "net/mac/tsch/tsch-slot-operation.h"
// #include "net/mac/tsch/tsch-queue.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/********** Global variables ***********/
#define UDP_PORT 8765

// network convergence time
#define CONVERGENCE_TIME (60 * 10)

// UDP packet sending interval in seconds
#define PACKET_SENDING_INTERVAL 60

// UDP packet payload size
#define UDP_PLAYLOAD_SIZE 9

/*#define SPECIAL_PAYLOAD_SIZE 50
#define SPECIAL_PERIOD (0.5 * CLOCK_SECOND)*/
PROCESS(data_rate, "Data Rate Checking process");
//uint8_t number_of_pakets_traffic_1 = 0;
uint8_t number_of_pakets_traffic_2 = 0;


// UDP communication process


PROCESS(node_udp_process, "UDP communicatio process");


AUTOSTART_PROCESSES(&node_udp_process, &data_rate);

// data to send to the server
unsigned char custom_payload[UDP_PLAYLOAD_SIZE];
//unsigned char special_payload[SPECIAL_PAYLOAD_SIZE];

// function to populate the payload
void create_payload()
{
  uint16_t i;
  for (i = 4; i < UDP_PLAYLOAD_SIZE; i++)
  {
    custom_payload[i] = i + 'a';
  }
  custom_payload[2] = 0xFF;
  custom_payload[3] = 0xFF;
  /*
  for (uint16_t i = 2; i < SPECIAL_PAYLOAD_SIZE; i++){
    special_payload[i] = i + 'a';
  }*/
}


void print_own_ip() {
    //int i;
    uip_ds6_addr_t *addr;
    //uint8_t state;

    addr = uip_ds6_get_global(-1);
    if(addr != NULL){
      //LOG_INFO("Node global IPv6 address :\n");
      LOG_INFO_6ADDR(&(addr->ipaddr));
      //LOG_INFO("\n");
    }
    else{
      LOG_ERR("Could not get IPv6 global address \n");
    }

    /*for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
           (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uip_ipaddr_copy(&ipaddr, &uip_ds6_if.addr_list[i].ipaddr);
            LOG_INFO_6ADDR(&ipaddr);
            LOG_INFO_6ADDR("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
            ipaddr.u8[0], ipaddr.u8[1], ipaddr.u8[2], ipaddr.u8[3],
            ipaddr.u8[4], ipaddr.u8[5], ipaddr.u8[6], ipaddr.u8[7],
            ipaddr.u8[8], ipaddr.u8[9], ipaddr.u8[10], ipaddr.u8[11],
            ipaddr.u8[12], ipaddr.u8[13], ipaddr.u8[14], ipaddr.u8[15]);
            LOG_INFO("\n");
        }
    }*/
}

// function to receive udp packets
static void rx_packet(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr,
                      uint16_t sender_port, const uip_ipaddr_t *receiver_addr,
                      uint16_t receiver_port, const uint8_t *data, uint16_t datalen)
{
  //uint8_t id = sender_addr->u8[15];
  uint8_t size = UDP_PLAYLOAD_SIZE;
  /*if (id == 2 || id == 9 || id == 58 || id == 65){ 
  	number_of_pakets_traffic_1++;
  	size = SPECIAL_PAYLOAD_SIZE;
  } else {*/
  	number_of_pakets_traffic_2++;
  //}
  
  char received_data[size];
  memcpy(received_data, data, datalen);

  uint16_t packet_num;
  packet_num = received_data[1] & 0xFF;
  packet_num = (packet_num << 8) + (received_data[0] & 0xFF);
  
  LOG_INFO("Received_from %" PRIu16 " packet_number: %" PRIu16 " ; current_time = %" PRIu64 "\n", sender_addr->u8[15], packet_num, tsch_get_network_uptime_ticks());
  // LOG_INFO_6ADDR(sender_addr);
}

/********** UDP Communication Process - Start ***********/
PROCESS_THREAD(node_udp_process, ev, data)
{
  static struct simple_udp_connection udp_conn;
  static struct etimer periodic_timer;

  static uint16_t seqnum;
  uip_ipaddr_t dst;

  PROCESS_BEGIN();

  // creating the payload
  create_payload();

  /* Initialization; `rx_packet` is the function for packet reception */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, rx_packet);

  if (node_id == MAIN_GW_ID)
  { /* node_id is 1, then start as root*/
    NETSTACK_ROUTING.root_start();
  }
  // Initialize the mac layer
  NETSTACK_MAC.on();

  // if this is a simple node, start sending upd packets
  LOG_INFO("Started UDP communication\n");

  if (node_id != MAIN_GW_ID){ // != 1
    // wait till network is converged
    etimer_set(&periodic_timer, CONVERGENCE_TIME * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
    // start the timer for periodic udp packet sending
    etimer_set(&periodic_timer, (PACKET_SENDING_INTERVAL + node_id) * CLOCK_SECOND);
    
    /* Main UDP comm Loop */
    while (1)
    { 
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

      if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
      {
        /* Send the packet number to the root and extra data */
        custom_payload[0] = seqnum & 0xFF;
        custom_payload[1] = (seqnum >> 8) & 0xFF;
        LOG_INFO("Sent_to %" PRIu16 " packet_number: %" PRIu16" ; current_time = %" PRIu64 "\n", dst.u8[15], seqnum, tsch_get_network_uptime_ticks());
        /*LOG_INFO("Sent_to ");
        LOG_INFO_6ADDR(&dst);
        LOG_INFO(" from ");
        print_own_ip();
        LOG_INFO(" packet_number: %" PRIu16 " ; current_time = %" PRIu64 "\n",seqnum, tsch_get_network_uptime_ticks());*/
        simple_udp_sendto(&udp_conn, &custom_payload, UDP_PLAYLOAD_SIZE, &dst);
        seqnum++;
      }
      else {
        LOG_INFO("Destination unreachable ! \n");
      }
      etimer_set(&periodic_timer, (random_rand()%11 + PACKET_SENDING_INTERVAL - 10) * CLOCK_SECOND);
    }
  }
  PROCESS_END();
}
/********** UDP Communication Process - End ***********/

PROCESS_THREAD(data_rate, ev, data)
{
  static struct etimer my_timer;
  PROCESS_BEGIN();
  if (node_id == 1){
  etimer_set(&my_timer, CONVERGENCE_TIME);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&my_timer));
 
  etimer_set(&my_timer, 12 * CLOCK_SECOND);
  
  while (1){
     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&my_timer));
     /*LOG_INFO("Data_Rate_traffic_1:%d Num_packets:%d\n", (number_of_pakets_traffic_1 * 40), number_of_pakets_traffic_1);
     number_of_pakets_traffic_1 = 0;*/
     LOG_INFO("Data_Rate_traffic_2:%d Num_packets:%d\n", (number_of_pakets_traffic_2 * 8), number_of_pakets_traffic_2);
     number_of_pakets_traffic_2 = 0;
     etimer_set(&my_timer, 10 * CLOCK_SECOND);
  }
  }
  PROCESS_END();
}
