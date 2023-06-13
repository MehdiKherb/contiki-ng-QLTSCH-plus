#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */

/* USB serial takes space, free more space elsewhere */
#define SICSLOWPAN_CONF_FRAG 0
#define UIP_CONF_BUFFER_SIZE 160

/*******************************************************/
/******************* Configure TSCH ********************/
/*******************************************************/
#define QUEUEBUF_CONF_NUM 8
#define TSCH_CONF_MAX_INCOMING_PACKETS 8
#define TSCH_CONF_MAC_MAX_FRAME_RETRIES 3

/* Disable the 6TiSCH minimal schedule */
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0

#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 64 /* No need for routes */
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS 64 
/* IEEE802.15.4 PANID */
#define IEEE802154_CONF_PANID 0x81a5

/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
// you can use this funtion to finish initialization
#define TSCH_CONF_AUTOSTART 0

// UDP packet sending interval in seconds
#define PACKET_SENDING_INTERVAL 60

// UDP packet payload size
#define UDP_PLAYLOAD_SIZE 10

// MAX number of re-transmissions
// old #define TSCH_CONF_MAX_FRAME_RETRIES 0

// hopping sequence
//#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_2_2

#define SIMPLE_ENERGEST_CONF_PERIOD (10 * CLOCK_SECOND)

// Default slotframe length
// #define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH 7

// Packetbuffer size
// #define PACKETBUF_CONF_SIZE 128 // 128 default

// The 6lowpan "headers" length
// #define TSCH_CONF_WITH_SIXTOP 1

/*******************************************************/
#if WITH_SECURITY
/* Enable security */
#define LLSEC802154_CONF_ENABLED 1
#endif /* WITH_SECURITY */

/*******************************************************/
/************* Other system configuration **************/
/*******************************************************/

/* Logging */
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_ERR // alternative _INFO/WARN
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_ERR
#define TSCH_LOG_CONF_PER_SLOT 0

#endif /* PROJECT_CONF_H_ */
