/*
 * Data collection test.
 * Nodes periodically generate traffic, the root collects it.
 */
#include "contiki.h"
#include "sys/node-id.h"
#include "lib/random.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/simple-udp.h"
#include "net/routing/routing.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SPECIAL_PAYLOAD_SIZE 50
#define SPECIAL_PERIOD (0.5 * CLOCK_SECOND)
#define UDP_PAYLOAD_SIZE 50

// data to send to the server
unsigned char custom_payload[UDP_PAYLOAD_SIZE];
unsigned char special_payload[SPECIAL_PAYLOAD_SIZE];

uint8_t number_of_pakets_traffic_1 = 0;
uint8_t number_of_pakets_traffic_2 = 0;

// function to populate the payload
void create_payload()
{
  for (uint16_t i = 4; i < UDP_PAYLOAD_SIZE; i++)
  {
    custom_payload[i] = i + 'a';
  }
  custom_payload[2] = 0xFF;
  custom_payload[3] = 0xFF;

  for (uint16_t i = 2; i < SPECIAL_PAYLOAD_SIZE; i++){
    special_payload[i] = i + 'a';
  }
}

/*---------------------------------------------------------------------------*/
static struct simple_udp_connection udp_conn;
static uip_ipaddr_t anycast_address;
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  // uint32_t seqnum;
  // memcpy(&seqnum, data, sizeof(seqnum));
  // LOG_INFO("seqnum=%" PRIu32 " from=", seqnum);
  // LOG_INFO_6ADDR(sender_addr);
  // LOG_INFO_("\n");
  char received_data[UDP_PAYLOAD_SIZE];
  memcpy(received_data, data, datalen);

  uint16_t packet_num;
  packet_num = received_data[1] & 0xFF;
  packet_num = (packet_num << 8) + (received_data[0] & 0xFF);

  LOG_INFO("Received_from %d packet_number: %d\n", sender_addr->u8[15], packet_num);
}
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "Node");
PROCESS(data_rate, "Data Rate Checking process");

AUTOSTART_PROCESSES(&node_process, &data_rate);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer periodic_timer;
  static uint32_t seqnum;

  uip_ipaddr_t dst;

  PROCESS_BEGIN();

  // creating the payload
  create_payload();

  printf("FIRMWARE_TYPE=%u\n", FIRMWARE_TYPE);
  printf("ORCHESTRA_CONF_UNICAST_PERIOD=%u\n", ORCHESTRA_CONF_UNICAST_PERIOD);
  printf("SEND_INTERVAL_SEC=%u\n", SEND_INTERVAL_SEC);

  uip_ip6addr(&anycast_address, UIP_DS6_DEFAULT_PREFIX, 0x0, 0x0, 0x0, 0x1, 0x2, 0x3, 0x4);

  simple_udp_register(&udp_conn, UDP_PORT, NULL,
                      UDP_PORT, udp_rx_callback);

  if(node_id == MAIN_GW_ID) {
    uip_ds6_addr_t *addr;

    /* Add the local anycast address */
    addr = uip_ds6_addr_add(&anycast_address, 0, ADDR_MANUAL);
    if(addr == NULL) {
      LOG_ERR("***  initialization: failed to add local anycast address!\n");
    }

    /* RPL root automatically becomes coordinator */  
    LOG_INFO("set as root\n");
    NETSTACK_ROUTING.root_start();
  }

  /* do it here because of the root rule */
  extern void orchestra_init(void);
  orchestra_init();
  
  /* start TSCH */
  NETSTACK_MAC.on();

  LOG_INFO("collection-exp node started\n");

  etimer_set(&periodic_timer, WARM_UP_PERIOD_SEC * CLOCK_SECOND + random_rand() % (SEND_INTERVAL_SEC * CLOCK_SECOND));
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

  if(node_id != MAIN_GW_ID) {
    if (node_id == 2 || node_id == 9 || node_id == 58 || node_id == 65) {
      // start the timer for periodic udp packet sending
      etimer_set(&periodic_timer, SPECIAL_PERIOD);
      
      /* Main UDP comm Loop */
      while (1)
      { 
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

        if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
        {
          /* Send the packet number to the root and extra data */
          special_payload[0] = seqnum & 0xFF;
          special_payload[1] = (seqnum >> 8) & 0xFF;
          LOG_INFO("Sent_to %d packet_number: %d\n", dst.u8[15], seqnum);
          // LOG_INFO_6ADDR(&dst);
          simple_udp_sendto(&udp_conn, &special_payload, SPECIAL_PAYLOAD_SIZE, &dst);
          seqnum++;
        }
        etimer_set(&periodic_timer, SPECIAL_PERIOD);
      }
    }
    else {
      // seqnum++;
      // simple_udp_sendto(&udp_conn, &seqnum, sizeof(seqnum), &anycast_address);
      // start the timer for periodic udp packet sending
      etimer_set(&periodic_timer, (SEND_INTERVAL_SEC + node_id) * CLOCK_SECOND);
      
      /* Main UDP comm Loop */
      while (1)
      { 
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

        if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
        {
          /* Send the packet number to the root and extra data */
          custom_payload[0] = seqnum & 0xFF;
          custom_payload[1] = (seqnum >> 8) & 0xFF;
          LOG_INFO("Sent_to %d packet_number: %d\n", dst.u8[15], seqnum);
          // LOG_INFO_6ADDR(&dst);
          simple_udp_sendto(&udp_conn, &custom_payload, UDP_PAYLOAD_SIZE, &dst);
          seqnum++;
        }
        etimer_set(&periodic_timer, (random_rand()%11 + SEND_INTERVAL_SEC - 10) * CLOCK_SECOND);
      }
    } 
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(data_rate, ev, data)
{   
  static struct etimer my_timer;
  PROCESS_BEGIN();
  if (node_id == 1){
  etimer_set(&my_timer, WARM_UP_PERIOD_SEC * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&my_timer));
 
  etimer_set(&my_timer, 12 * CLOCK_SECOND);
  
  while (1){
     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&my_timer));
     LOG_INFO("Data_Rate_traffic_1:%d Num_packets:%d\n", (number_of_pakets_traffic_1 * 40), number_of_pakets_traffic_1);
     number_of_pakets_traffic_1 = 0;
     LOG_INFO("Data_Rate_traffic_2:%d Num_packets:%d\n", (number_of_pakets_traffic_2 * 8), number_of_pakets_traffic_2);
     number_of_pakets_traffic_2 = 0;
     etimer_set(&my_timer, 10 * CLOCK_SECOND);
  }
  }
  PROCESS_END();
}
