/********** Libraries ***********/
#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "sys/node-id.h"

#include "net/mac/tsch/tsch-slot-operation.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-schedule.h"
// My additional libraries
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"

#include "net/routing/rpl-classic/rpl.h"
#include "net/routing/rpl-classic/rpl-private.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include <stdlib.h>
#include "lib/memb.h"
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/********** Global variables ***********/
#define UDP_PORT 8765
#define UDP_PORT_BROADCAST 8099


// period to to check Tx status
#define TIME_10MS (CLOCK_SECOND * 0.01)

// number to update APT values 0 < num < 1 (APT values will decay to 0s)
#define APT_UPDATE_NUMBER 0.999

// Tx slot status checking time interval in 10ms
#define TX_SLOT_STATUS_CHECK 1

#define SPECIAL_PAYLOAD_SIZE 50
#define SPECIAL_PERIOD (0.5 * CLOCK_SECOND)
PROCESS(data_rate, "Data Rate Checking process");
uint8_t number_of_pakets_traffic_1 = 0;
uint8_t number_of_pakets_traffic_2 = 0;

// UDP communication process
PROCESS(node_udp_process, "UDP communicatio process");
// Q-Learning and scheduling process
PROCESS(scheduler_process, "QL-TSCH Scheduler Process");
// APT updating process
PROCESS(apt_update_process, "APT Updating Process");

PROCESS(udp_listener_process, "UDP broadcast listener process");

AUTOSTART_PROCESSES(&node_udp_process, &scheduler_process, &apt_update_process, &data_rate, &udp_listener_process);

// data to send to the server
unsigned char custom_payload[UDP_PLAYLOAD_SIZE];
unsigned char special_payload[SPECIAL_PAYLOAD_SIZE];

// Broadcast slotframe and Unicast slotframe
struct tsch_slotframe *sf_broadcast;
struct tsch_slotframe *sf_unicast;

// array to store the links of the unicast slotframe
// struct tsch_link *links_unicast_sf[UNICAST_SLOTFRAME_LENGTH];

// a variable to store the current action number
uint8_t current_action = 0;

// array to store Q-values of the actions (or timeslots)
float q_values[UNICAST_SLOTFRAME_LENGTH];

// reward values
int reward_succes = 0;
int reward_failure = -1;

// cycles since the beginning of the first slotframe
uint16_t cycles_since_start = 0;

// epsilon-greedy probability
float epsilon_fixed = 0.5;

// Q-learning parameters
float learning_rate = 0.1;
float discount_factor = 0.95;

// Tx status and timeslot number
uint8_t *Tx_status;


/********* Extended List API to manage child nodes **********/

#define MAX_CHILD_NODES 30
// node struct to use with the contiki list API
struct node {
  struct node *next;
  uip_ipaddr_t data;
  uint8_t rx_timeslot;
};
// Pre-allocated space for child nodes
MEMB(childs_memb, struct node, MAX_CHILD_NODES);
// declaring the list of child nodes
LIST(child_nodes);

// initialize the list of child nodes
void init_childs_list(){
  memb_init(&childs_memb);
  list_init(child_nodes);
}

// Function to add a node to the list
void add(uip_ipaddr_t data, uint8_t rx_timeslot){
  //struct node *n = (struct node *)malloc(sizeof(struct node));
  LOG_DBG("Adding child node %d \n", data.u8[15]);
  struct node *n = memb_alloc(&childs_memb);
  if(n != NULL){
    n->data = data;
    n->rx_timeslot = rx_timeslot;
    list_add(child_nodes, n);
    LOG_DBG("Node added to the list\n");
  }
  else {
    LOG_ERR("Memory allocation failed, cannot add more elements.\n");
  }
}

struct node *getNode(uip_ipaddr_t data) {
  struct node *n;
  for(n = list_head(child_nodes); n != NULL; n = list_item_next(n)) {
    if(n->data.u8[15] == data.u8[15]){
      return n;
    }
  }
  return NULL;
}
// Function to check if a node already exists in the list
int exists(uip_ipaddr_t data) {
  LOG_DBG("Checking if the node %d exists\n", data.u8[15]);
  struct node *n;
  for(n = list_head(child_nodes); n != NULL; n = list_item_next(n)) {
    if(n->data.u8[15] == data.u8[15]){
      LOG_DBG("Node exists \n");
      return 1;  // Return 1 (true) if a node with the given data is found
    }
  }
  LOG_DBG("Node does not exist\n");
  return 0;  // Return 0 (false) if no such node is found
}

int rx_timeslot_exists(uint8_t rx_timeslot){
  struct node *n;
  for(n = list_head(child_nodes); n != NULL; n = list_item_next(n)) {
    if(n->rx_timeslot == rx_timeslot){
      LOG_DBG("Node with rx timeslot %u exists \n", rx_timeslot);
      return 1;  // Return 1 (true) if a node with the given data is found
    }
  }
  LOG_DBG("Node with rx timeslot %u does not exist\n", rx_timeslot);
  return 0;  // Return 0 (false) if no such node is found
}

// Function to update the timeslot of a node
void update_timeslot(uip_ipaddr_t data, uint8_t new_rx_timeslot) {
  struct node *n;
  for(n = list_head(child_nodes); n != NULL; n = list_item_next(n)) {
    if(n->data.u8[15] == data.u8[15]){
      n->rx_timeslot = new_rx_timeslot;
      return;
    }
  }
  LOG_ERR("Node with the same last byte of the IP address not found in the list.\n");
}

// Function to display the child nodes
void displayChildNodes() {
  struct node *n;
  LOG_INFO("Child nodes : ");
  for(n = list_head(child_nodes); n != NULL; n = list_item_next(n)) {
    //LOG_INFO_6ADDR(&n->data);
    LOG_INFO("Node %d, Timeslot: %u |",n->data.u8[15], n->rx_timeslot);
  }
  LOG_INFO("\n");
}

/***********************************************************/

//#define INFINITE_RANK 1000
// check if an address correponds to the node's preferred rpl parent
/*
int is_preferred_parent(const uip_ipaddr_t *ip_address) {
  rpl_dag_t *dag = rpl_get_any_dag();
  if (dag != NULL && dag->preferred_parent != NULL) {
    uip_ds6_nbr_t *nbr = rpl_get_nbr(dag->preferred_parent);
    if (nbr != NULL) {
      return uip_ipaddr_cmp(&nbr->ipaddr, ip_address);
    }
  }
  return 0;
}
*/

// check if an address correponds to one of our child nodes in RPL

int is_child_node(const uip_ipaddr_t *ipaddr) {
  rpl_dag_t *dag;
  rpl_parent_t *parent;
  uip_ipaddr_t parent_ipaddr;

  if (ipaddr == NULL) {
    return 0;
  }
  
  dag = rpl_get_any_dag();
  if(dag == NULL) {
    return 0;
  }
  LOG_DBG("Checking if child .. \n");
  // Iterate through RPL parents
  for (parent = nbr_table_head(rpl_parents); parent != NULL; parent = nbr_table_next(rpl_parents, parent)) {
    uip_ipaddr_copy(&parent_ipaddr, rpl_parent_get_ipaddr(parent));

        // Get the IPv6 address of the RPL parent
        /*uip_ipaddr_t parent_ipaddr;
        uip_ds6_set_addr_iid(&parent_ipaddr, (uip_lladdr_t *)uip_ds6_nbr_get_ll(nbr));*/
        //LOG_INFO("checking parent %d against %d\n", parent_ipaddr.u8[15], ipaddr->u8[15]);
        //if (uip_ipaddr_cmp(&parent_ipaddr, ipaddr)) {
        if (parent_ipaddr.u8[15] == ipaddr->u8[15]){
          if (dag->rank < parent->rank) {
            LOG_DBG("neighbor %d is our child node\n", parent_ipaddr.u8[15]);
            return 1;
          }
        }
  }
  LOG_DBG("Not a child\n");
  return 0;
}

// given a node's address, check if it is our preferred parent..
/*uint8_t is_preferred_parent(const uip_ipaddr_t *ipaddr) {
  rpl_dag_t *dag;
  uip_ipaddr_t preferred_parent_ipaddr;

  dag = rpl_get_any_dag();
  if(dag == NULL || dag->preferred_parent == NULL) {
    return 0;
  }

  uip_ipaddr_copy(&preferred_parent_ipaddr, rpl_parent_get_ipaddr(dag->preferred_parent));

  if(uip_ipaddr_cmp(&preferred_parent_ipaddr, ipaddr)) {
    return 1;
  }

  return 0;
}*/

// update rx timeslots allocation.
/*
void update_rx_timeslots() {
  //LOG_DBG("Entered update_rx_timeslots\n");
  struct tsch_link *l;
  struct tsch_link *next_l;
  
  struct node *child = list_head(child_nodes);
  // First, remove all existing RX timeslots that are not also TX timeslots and do not match any of the childs rx timeslots
  //if(!tsch_is_locked())
  if(tsch_get_lock()) {
    LOG_DBG("Got lock for rx timeslots update\n");
    l = list_head(sf_unicast->links_list); 
    while(l != NULL){

      // Save the next link, as we might delete this one
      next_l = list_item_next(l);
      uint8_t not_remove = 0;
      // If this is an RX link and not a TX link and does not match one of the childs rx timeslots, remove it
      if((l->link_options & LINK_OPTION_RX) && !(l->link_options & LINK_OPTION_TX)) {
          child = list_head(child_nodes);
          //if a link corresponds to one of the childs rx timeslot -> do not remove it
          while(child != NULL){
            if(child->rx_timeslot == l->timeslot){
              not_remove = 1;
              break;
            }
            child = list_item_next(child_nodes);
          }
          if(not_remove == 0) {
            tsch_release_lock();
            tsch_schedule_remove_link(sf_unicast, l);
          }
      }
      l = next_l;
    }
  }
  else LOG_DBG("TSCH is locked\n"); 
  

  // Then, add new RX timeslots for each child node
  child = list_head(child_nodes);
  while(child != NULL){
    l = tsch_schedule_get_link_by_timeslot(sf_unicast, child->rx_timeslot, 0);
    if(l != NULL){ // if a link exists for the child rx timeslot
      if (!(l->link_options & LINK_OPTION_RX)){ // if it is not Rx
      tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                   LINK_TYPE_NORMAL, &tsch_broadcast_address, child->rx_timeslot, 0, 0);
      }
    }
    else { // if a link does not exist at the child rx timeslot
      tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                   LINK_TYPE_NORMAL, &tsch_broadcast_address, child->rx_timeslot, 0, 0);
    }
    child = list_item_next(child_nodes);
  }
} 
*/
// Simple udp connection for broadcasting the chosen timeslot to neighbors (chosen action)
static struct simple_udp_connection udp_connection_broadcast;


// function to send the broadcast message with the chosen time slot
void send_broadcast_message(uint8_t chosen_timeslot) {
  uip_ipaddr_t addr;

  // Set the destination IP address to the IPv6 link-local all-nodes multicast address
  uip_create_linklocal_allnodes_mcast(&addr);
 
  // Send the broadcast message with the chosen time slot
  //LOG_INFO("Sending timeslot %d\n", chosen_timeslot);
  simple_udp_sendto(&udp_connection_broadcast, &chosen_timeslot, sizeof(uint8_t), &addr);
}


// UDP reception callback function to handle received broadcast messages and update the APT accordingly
static void udp_recv_callback(struct simple_udp_connection *c,
                              const uip_ipaddr_t *sender_addr,
                              uint16_t sender_port,
                              const uip_ipaddr_t *receiver_addr,
                              uint16_t receiver_port,
                              const uint8_t *data,
                              uint16_t datalen) {
  uint8_t received_time_slot = *data;

  LOG_DBG("Received broadcasted timeslot %u from %d\n", received_time_slot, sender_addr->u8[15]);


  if(is_child_node(sender_addr)) {
    if (exists(*sender_addr)){
      //displayChildNodes();
      //LOG_INFO("Node %d exists, updating its timeslot\n", sender_addr->u8[15]);
      struct node *child = getNode(*sender_addr);
      if(child != NULL){
        LOG_DBG("got Node %d with timeslot %u\n", child->data.u8[15], child->rx_timeslot);
        // save the value of the old rx timeslot for the child
        uint8_t old_rx_timeslot = child->rx_timeslot; 
        // update the rx timeslot of the child
        if(old_rx_timeslot != received_time_slot){
          update_timeslot(*sender_addr, received_time_slot); 

          struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_unicast, old_rx_timeslot, 0);
          // to remove the old_rx_timeslot 
          if(l != NULL){
            if((l->link_options & LINK_OPTION_TX) && (rx_timeslot_exists(old_rx_timeslot))){
              LOG_DBG("[callback_exists_remove] The link %u cannot be removed as it has Tx and another child Rx is scheduled here\n", old_rx_timeslot);
            }
            else { // !Tx or !rx_timeslot_exists
              if((l->link_options & LINK_OPTION_TX) && !(rx_timeslot_exists(old_rx_timeslot))){

                tsch_schedule_remove_link_by_timeslot(sf_unicast, old_rx_timeslot, 0);
                tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                          LINK_TYPE_NORMAL, &tsch_broadcast_address, old_rx_timeslot, 0, 0);
                LOG_DBG("[callback_exists_remove] The link %u has been switched to only Tx\n", old_rx_timeslot);
              }
              else { // !Tx or rx_timeslot_exists
                if ((rx_timeslot_exists(old_rx_timeslot)) && !(l->link_options & LINK_OPTION_TX)) {
                  LOG_DBG("[callback_exists_remove] The link %u cannot be removed as another child Rx is scheduled here\n", old_rx_timeslot);
                }
                else // if the old rx timeslot is a not a tx and there is not another node having this link as rx, remove it 
                  if (!(rx_timeslot_exists(old_rx_timeslot)) && !(l->link_options & LINK_OPTION_TX)){
                    tsch_schedule_remove_link_by_timeslot(sf_unicast, old_rx_timeslot, 0);
                    LOG_DBG("[callback_exists_remove] The link %u has been removed\n", old_rx_timeslot);
                  }
              }
            }
          }
          else LOG_DBG("[callback_exists_remove] No link with timeslot %d is scheduled\n",old_rx_timeslot);
        }
        
      }
      
      
      // allocate the rx timeslot for the sender child if it is different than the old rx timeslot
      //if(old_rx_timeslot != received_time_slot){
        // check if there is a link already scheduled at the received_time_slot 
        struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_unicast, received_time_slot, 0);
        if(l != NULL){
          if((l->link_options & LINK_OPTION_TX)){ // if it has Tx flag, set an Rx/Tx timeslot
            
            tsch_schedule_remove_link_by_timeslot(sf_unicast, received_time_slot, 0);
            tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                     LINK_TYPE_NORMAL, &tsch_broadcast_address, received_time_slot, 0, 0);
            LOG_DBG("[callback_exists_allocate] Link with timeslot %u have Tx, set Tx/Rx link\n", received_time_slot);
          }
          else { // if it does not have Tx flag, set only an Rx timeslot
            // add only if there is not an Rx already scheduled, to avoid duplicates
            if(!(l->link_options & LINK_OPTION_RX)){
              LOG_DBG("[callback_exists_allocate] Scheduled Rx timeslot at %u \n", received_time_slot);
              tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                       LINK_TYPE_NORMAL, &tsch_broadcast_address, received_time_slot, 0, 0);
            }
          }
        }
        else { // if no link is scheduled at the received_time_slot ..
          tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                     LINK_TYPE_NORMAL, &tsch_broadcast_address, received_time_slot, 0, 0);
          LOG_DBG("[callback_exists_allocate] No link with timeslot %d is scheduled, add Rx\n", received_time_slot);
        }
       
    }
    else{ // child node does not exist in the list, adding ..
      //LOG_DBG("Node %d does not exist, adding ..\n", sender_addr->u8[15]);
      
      add(*sender_addr, received_time_slot);
      //displayChildNodes();
      //check if a link is already there so we don't superpose it and cause inconsistencies..
      struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_unicast, received_time_slot, 0); 
      if(l != NULL){
        if(!(l->link_options & LINK_OPTION_RX) && (l->link_options & LINK_OPTION_TX)) {// the link does not have Rx flag and has Tx
          
          tsch_schedule_remove_link_by_timeslot(sf_unicast, received_time_slot, 0);
          tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                   LINK_TYPE_NORMAL, &tsch_broadcast_address, received_time_slot, 0, 0);
          LOG_DBG("[callback_Add_allocate] Link with timeslot %d have Tx flag, set Tx/Rx link\n", received_time_slot);
        
        }
      }
      else { // if no link is scheduled at the received time slot
        
        tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                   LINK_TYPE_NORMAL, &tsch_broadcast_address, received_time_slot, 0, 0);
        LOG_DBG("[callback_Add_allocate] No link with timeslot %d is scheduled, add Rx\n", received_time_slot);
      
      }

    }
    
    
    //if(tsch_get_lock()){
    /*LOG_DBG("Updating Rx timeslots\n");
    update_rx_timeslots();
    LOG_DBG("Updated rx timeslots\n");*/
    // tsch_release_lock();
    //}
    //else LOG_INFO("Failed to get lock to update rx timeslots \n");

    
    //tsch_schedule_print();
    //displayChildNodes();
  }
  // Update the APT
  //LOG_DBG("Incrementing APT at %u\n", received_time_slot);
  increment_apt_value(received_time_slot);
  LOG_DBG("Incremented APT\n");
}


// Set up the initial schedule
static void init_tsch_schedule(void)
{
  // delete all the slotframes
  tsch_schedule_remove_all_slotframes();

  // create a broadcast slotframe and a unicast slotframe
  sf_broadcast = tsch_schedule_add_slotframe(0, BROADCAST_SLOTFRAME_LENGTH);
  sf_unicast = tsch_schedule_add_slotframe(1, UNICAST_SLOTFRAME_LENGTH);

  // shared/advertising cell at (0, 0) --> create a shared/advertising link in the broadcast slotframe
  tsch_schedule_add_link(sf_broadcast, LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED,
                         LINK_TYPE_ADVERTISING, &tsch_broadcast_address, 0, 0, 0);
  // shared/advertising cell at (1, 0) dedicated for broadcasting the chosen timeslot 
  tsch_schedule_add_link(sf_broadcast, LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED,
                         LINK_TYPE_ADVERTISING, &tsch_broadcast_address, 1, 0, 0);


  if(node_id == 1){ // if the node is the sink
  // create multiple Rx links in the rest of the unicast slotframe
    for (int i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++)
    {
      tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                   LINK_TYPE_NORMAL, &tsch_broadcast_address, i, 0, 0);
    }
  }
  else {  
    // create one Tx link in the first slot of the unicast slotframe
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                LINK_TYPE_NORMAL, &tsch_broadcast_address, 0, 0, 0);
    // Allocate randomly one Rx link 
    /*rx_timeslot = random_rand() % UNICAST_SLOTFRAME_LENGTH;
    while (rx_timeslot == 0){
      rx_timeslot = random_rand() % UNICAST_SLOTFRAME_LENGTH;
    }
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                 LINK_TYPE_NORMAL, &tsch_broadcast_address, rx_timeslot, 0, 0);*/
  }
  
}

// set up new schedule based on the chosen action
void set_up_new_schedule(uint8_t action)
{ 
  if (action != current_action)
  {
    struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_unicast, current_action, 0);
    // to remove the old tx timeslot (current_action)
    // check if there is an Rx timeslot already scheduled 
    if(l != NULL){

        if((l->link_options & LINK_OPTION_RX)) { 
          LOG_DBG("[set_new_schedule_remove] Link with timeslot %d have Rx flag, switch it to Rx only\n", current_action);
          tsch_schedule_remove_link_by_timeslot(sf_unicast, current_action, 0);
          tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                      LINK_TYPE_NORMAL, &tsch_broadcast_address, current_action, 0, 0);
        }
        else {
          LOG_DBG("[set_new_schedule_remove] Remove Tx link with timeslot %d\n", current_action);
          tsch_schedule_remove_link_by_timeslot(sf_unicast, current_action, 0);
        }
    }
    else LOG_DBG("No link with timeslot %d is scheduled\n", current_action);
    
    l = tsch_schedule_get_link_by_timeslot(sf_unicast, action, 0);
    // to add the newly chosen timeslot (action)
    // check if a link is already scheduled at the newly chosen timeslot..
    if(l != NULL){
      if((l->link_options & LINK_OPTION_RX)){ // if it has Rx flag , set to Tx | Rx timeslot
        LOG_DBG("[set_new_schedule_allocate] Link with timeslot %d have Rx flag, set Tx/Rx link\n", action);
        tsch_schedule_remove_link_by_timeslot(sf_unicast, action, 0);
        tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_RX |
                                                          LINK_OPTION_SHARED, LINK_TYPE_NORMAL, &tsch_broadcast_address, action, 0, 0);
      }
      else{ // if it does not have Rx flag
        LOG_DBG("[set_new_schedule_allocate] Add Tx link with timeslot %d\n", action);
        tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                      LINK_TYPE_NORMAL, &tsch_broadcast_address, action, 0, 0);

      } 
    }
    else{// if no link is scheduled at the newly chosen timeslot
      LOG_DBG("[set_new_schedule_allocate] Add Tx link with timeslot %d\n", action);
      tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                      LINK_TYPE_NORMAL, &tsch_broadcast_address, action, 0, 0);

    } 
    
    //tsch_schedule_print();
    
    // Send the broadcast message with the chosen time slot 
    if(node_id != 1){
        send_broadcast_message(action);
    }
    current_action = action;
  }
}

// function to populate the payload
void create_payload()
{
  for (uint16_t i = 4; i < UDP_PLAYLOAD_SIZE; i++)
  {
    custom_payload[i] = i + 'a';
  }
  custom_payload[2] = 0xFF;
  custom_payload[3] = 0xFF;
  
  for (uint16_t i = 2; i < SPECIAL_PAYLOAD_SIZE; i++){
    special_payload[i] = i + 'a';
  }
}

// initialize q-values randomly or set all to 0
void initialize_q_values(uint8_t val)
{
  if (val){
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      q_values[i] = (float) random_rand()/RANDOM_RAND_MAX;
    }
  } else {
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      q_values[i] = 0;
    }
  }
}

// choose exploration/explotation ==> 1/0 (gradient-greedy function)
uint8_t policy_check()
{ 
  float num = (float) random_rand()/RANDOM_RAND_MAX;
  float epsilon_new = (10000.0 / cycles_since_start);
  if (epsilon_new > epsilon_fixed) epsilon_new = epsilon_fixed;
  
  if (num < epsilon_new)
      return 1;
  else
      return 0;  
}

// function to find the highest Q-value and return its index
uint8_t max_q_value_index()
{
  uint8_t max = random_rand()%UNICAST_SLOTFRAME_LENGTH;
  for (uint8_t i = 1; i < UNICAST_SLOTFRAME_LENGTH; i++)
  {
    if (q_values[i] > q_values[max])
    {
      max = i;
    }
  }
  return max;
}

// Update q-value table function
void update_q_table(uint8_t action, int reward)
{ 
  uint8_t max = max_q_value_index();
  float expected_max_q_value = q_values[max] + reward_succes;
  q_values[action] = (1 - learning_rate) * q_values[action] + 
                      learning_rate * (reward + discount_factor * expected_max_q_value -
                      q_values[action]);
}

// link selector function
/*int my_callback_packet_ready(void)
{
#if TSCH_WITH_LINK_SELECTOR
  uint8_t slotframe = 0;
  uint8_t channel_offset = 0;
  uint8_t timeslot = 0;
  

  // Check if the packet is a broadcast message
  if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_null)) {
    
    uint8_t *payload_data = (uint8_t *) packetbuf_dataptr();
    uint8_t payload_length = packetbuf_datalen();
    LOG_INFO("payload length = %u\n", payload_length);
    //channel_offset = 0;
    //check if it is the broadcast message for APT using the udp specific port number
    if (payload_data[10] == 20) { // detect the magic number which identifies the chosen timeslot broadcast packet
    // Handle broadcast message
      LOG_INFO("scheduled broadcast udp, sent timeslot = %02X\n", payload_data[11]);
      slotframe = 0;
      timeslot = 1;
    }
    else {
      LOG_INFO("scheduled broadcast TSCH RPL ..\n");
      slotframe = 0;
      timeslot = 0;
    }
  }
  else {

    char *ch = packetbuf_dataptr();
    uint8_t f0 = ch[0] & 0xFF, f1 = ch[1] & 0xFF, f2 = ch[2] & 0xFF, f3 = ch[3] & 0xFF;

    if (f0 == 126 && f1 == 247 && f2 == 0 && f3 == 225)
    {
      LOG_INFO("scheduled unicast\n");
      timeslot = current_action;
      slotframe = 1;
    }
  }

  // LOG_INFO("Packet header length: %u\n", packetbuf_hdrlen());
  // LOG_INFO("Packet data length: %u\n", packetbuf_datalen());


  packetbuf_set_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME, slotframe);
  packetbuf_set_attr(PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
  packetbuf_set_attr(PACKETBUF_ATTR_TSCH_CHANNEL_OFFSET, channel_offset);*/
//#endif /* TSCH_WITH_LINK_SELECTOR */
/*  return 1;
}*/
  
// function to receive udp packets
static void rx_packet(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr,
                      uint16_t sender_port, const uip_ipaddr_t *receiver_addr,
                      uint16_t receiver_port, const uint8_t *data, uint16_t datalen)
{
  uint8_t id = sender_addr->u8[15];
  uint8_t size = UDP_PLAYLOAD_SIZE;
  if (id == 2 || id == 9 || id == 58 || id == 65){ 
  	number_of_pakets_traffic_1++;
  	size = SPECIAL_PAYLOAD_SIZE;
  } else {
  	number_of_pakets_traffic_2++;
  }
  
  char received_data[size];
  memcpy(received_data, data, datalen);

  uint16_t packet_num;
  packet_num = received_data[1] & 0xFF;
  packet_num = (packet_num << 8) + (received_data[0] & 0xFF);
  
  LOG_INFO("Received_from %d packet_number: %d\n", sender_addr->u8[15], packet_num);
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
  // initialize q-values
  initialize_q_values(RANDOM_Q_VALUES);

  // set all the APT values to 0s at the beginning
  set_apt_values();
  // set up the initial schedule
  init_tsch_schedule();
  // init list of child nodes
  init_childs_list();
  
  //------------------------
  // NETSTACK_RADIO.off()
  // NETSTACK_MAC.off()
  //-----------------------
  
  /* Initialization; `rx_packet` is the function for packet reception */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, rx_packet);

  if (node_id == 1)
  { /* node_id is 1, then start as root*/
    NETSTACK_ROUTING.root_start();
  }
  // Initialize the mac layer
  NETSTACK_MAC.on(); 

  // if this is a simple node, start sending udp packets
  LOG_INFO("Started UDP communication\n");

  LOG_INFO("Current Config : USF = %d, BSF = %d\n", UNICAST_SLOTFRAME_LENGTH, BROADCAST_SLOTFRAME_LENGTH);
  
  if (node_id != 1){ // change ==0 to != 1
  if (node_id == 2 || node_id == 9 || node_id == 58 || node_id == 65)
  {
    // wait till network is converged
    etimer_set(&periodic_timer, CONVERGENCE_TIME);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
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
  
  } else {
    // wait till network is converged
    etimer_set(&periodic_timer, CONVERGENCE_TIME);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    
    // start the timer for periodic udp packet sending
    etimer_set(&periodic_timer, (PACKET_SENDING_INTERVAL + node_id) * CLOCK_SECOND);
    
    /* Main UDP comm Loop */
    while (1)
    { 
#if PRINT_Q_VALUES_AND_APT
    // print the Q-values
    LOG_INFO("Q-Values:");
    char float_str[10]; // Adjust the size of this buffer as needed
    //snprintf(float_str, sizeof(float_str), "%.4f", my_float);
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      snprintf(float_str, sizeof(float_str), "%.2f", q_values[i]);
      LOG_INFO_(" %u-> %s ,", i, float_str);
    }
    LOG_INFO_("\n");

    // print APT table values
    LOG_INFO("APT-Values:");
    float *table = get_apt_table();
    char float_str2[10]; // Adjust the size of this buffer as needed
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      snprintf(float_str2, sizeof(float_str2), "%.4f", table[i]);
      LOG_INFO_(" (%u -> %s)", i, float_str2);
    }
    LOG_INFO_("\n");
    LOG_INFO("Total frame cycles: %u\n", cycles_since_start);
#endif /* PRINT_Q_VALUES_AND_APT */

      // reset all the backoff windows for all the neighbours
      //custom_reset_all_backoff_exponents();
    
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

      if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
      {
        /* Send the packet number to the root and extra data */
        custom_payload[0] = seqnum & 0xFF;
        custom_payload[1] = (seqnum >> 8) & 0xFF;
        LOG_INFO("Sent_to %d packet_number: %d\n", dst.u8[15], seqnum);
        // LOG_INFO_6ADDR(&dst);
        simple_udp_sendto(&udp_conn, &custom_payload, UDP_PLAYLOAD_SIZE, &dst);
        seqnum++;
      }
      etimer_set(&periodic_timer, (random_rand()%11 + PACKET_SENDING_INTERVAL - 10) * CLOCK_SECOND);
    }
  }
  }
  PROCESS_END();
}
/********** UDP Communication Process - End ***********/

/********** QL-TSCH Scheduler Process - Start ***********/
PROCESS_THREAD(scheduler_process, ev, data)
{
  // timer to update the policy
  static struct etimer policy_update_timer;
  Tx_status = get_Tx_slot_status();
  
  PROCESS_BEGIN();
  if(node_id != 1) { 
      
      
      etimer_set(&policy_update_timer, CONVERGENCE_TIME);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));
      // wait untill the initial setupt finishes
      LOG_INFO("Starting Q-table Update Process - Setup finished\n");
      
      // set the timer for one whole frame cycle 
      etimer_set(&policy_update_timer, TIME_10MS * TX_SLOT_STATUS_CHECK);

      /* Main Scheduler Loop */
      while (1)
      { 
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));

    

        /**********  Q-value update calculations - Start **********/
     
        // updating the q-table based on the last action results
        if (Tx_status[0]) {
          etimer_set(&policy_update_timer, (TIME_10MS * (UNICAST_SLOTFRAME_LENGTH - 1 - Tx_status[1])));
          PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));
          if (Tx_status[0] == 1){
            //LOG_INFO("Tx success \n");
            update_q_table(current_action, reward_succes);
          } else {
            //LOG_INFO("Tx failure \n");
            update_q_table(current_action, reward_failure);
          }
          Tx_status[0] = 0;
        //} //------------ change is here -------------------
        
        // choosing exploration/exploitation and updating the schedule
          uint8_t action;
          if (policy_check() == 1){ /* Exploration */
            action = get_slot_with_apt_table_min_value();
           // LOG_INFO("Exploration %d\n", action);
          } else { /* Exploitation */
            action = max_q_value_index();
           // LOG_INFO("Exploitation %d\n", action);
          }
        
          // set up a new schedule based on the chosen action
          //if(tsch_get_lock()){
            LOG_DBG("Updating Tx timeslot\n");
            set_up_new_schedule(action);  
            LOG_DBG("Updated Tx timeslot\n");
            //tsch_release_lock();
          //}
          //else LOG_INFO("Failed to get lock to set up new schedule \n");
          
          //update_rx_timeslots();
        } //------------ and here -------------------

        
        // set the timer again -> duration 10 ms
        etimer_set(&policy_update_timer, TIME_10MS * TX_SLOT_STATUS_CHECK);
        cycles_since_start++;

        /**********  Q-value update calculations - End **********/
      }
  }
  PROCESS_END();
  
}
/********** QL-TSCH Scheduler Process - End ***********/

/********** APT Update Process - Start ***********/
PROCESS_THREAD(apt_update_process, ev, data)
{
  // timer to update APT
  static struct etimer apt_update_timer;
  uint16_t count = 0;
  PROCESS_BEGIN();
  if(node_id != 1){
    
    
    // wait untill the initial setupt finishes
    LOG_INFO("Starting APT Update Process - Setup finished\n");
    
    // set the timer for one whole frame cycle 
    etimer_set(&apt_update_timer, (TIME_10MS * UNICAST_SLOTFRAME_LENGTH));

    /* Main APT Update function loop */
    while (1)
    { 
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&apt_update_timer));
      update_apt_table(APT_UPDATE_NUMBER);
      count++;
      etimer_set(&apt_update_timer, (TIME_10MS * UNICAST_SLOTFRAME_LENGTH));
      if (count == 500){
      // reset all the backoff windows for all the neighbours
       custom_reset_all_backoff_exponents();
       count = 0;
      }
    }
  }   
  PROCESS_END();

}
/********** APT Update Process - end ***********/

PROCESS_THREAD(data_rate, ev, data)
{
  static struct etimer my_timer;
  PROCESS_BEGIN();
  if (node_id == 1){ // change 1 to 0
  etimer_set(&my_timer, CONVERGENCE_TIME);
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

/******** UDP broadcast Process *************/
PROCESS_THREAD(udp_listener_process, ev, data) {
  PROCESS_BEGIN();
  static struct etimer convergence_timer;
  etimer_set(&convergence_timer, CONVERGENCE_TIME - 10*CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&convergence_timer));
  
  // Set up the simple_udp_connection
  simple_udp_register(&udp_connection_broadcast,
                      UDP_PORT_BROADCAST, // Choose a suitable UDP port number
                      NULL,
                      UDP_PORT_BROADCAST,
                      udp_recv_callback);
  //etimer_set(&convergence_timer, CONVERGENCE_TIME);
  
  //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&convergence_timer));
  while (1) {
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}

/********************************************/