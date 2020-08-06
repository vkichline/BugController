#include <WiFi.h>
#include "BugComm.h"

// #define DEBUG_MSG_ON_DATA_SENT
// #define DEBUG_DUMP_PACKET


bool                BugComm::data_ready           = false;
bool                BugComm::data_valid           = false;
bool                BugComm::connected            = false;
uint8_t             BugComm::channel              = 0;
uint8_t             BugComm::response_len         = 0;
int8_t              BugComm::last_x               = 127;
int8_t              BugComm::last_y               = 127;
bool                BugComm::last_b               = false;
uint8_t             BugComm::responseAddress[6]   = { 0 };
uint8_t             BugComm::broadcastAddress[]   = BROADCAST_MAC_ADDRESS;
uint8_t             BugComm::peerAddress[6]       = { 0 };
BugComm_Kind        BugComm::msg_kind             = KIND_NONE;
BugComm_Mode        BugComm::device_mode          = MODE_UNINITIALIZED;
esp_now_peer_info_t BugComm::peerInfo;
BugComm_Command     BugComm::command;
BugComm_Response    BugComm::response;
BugComm_Discovery   BugComm::discovery;


// Set the mode that this device operates in.
//
void BugComm::begin(BugComm_Mode mode) {
  device_mode = mode;
}


// Make sure wifi is running bug idle by starting hidden access point and switching to station mode.
// Set WiFi channel to channel provided.
// Register callbacks and set the peer address (broadcast if omitted.)
// TODO: Handle startup w/ WiFi already running, switch channels w/ autoconnect.
//
bool BugComm::initialize_esp_now(uint8_t chan, uint8_t* mac) {
  channel = chan;
  WiFi.disconnect();
  WiFi.softAP(AP_NAME, "", channel, 1);   // Create a hidden AP on given channel
  WiFi.mode(WIFI_STA);                    // ...and switch to station mode
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  esp_now_register_send_cb(on_data_sent);
  esp_now_register_recv_cb(on_data_received);

  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  if (ESP_OK == esp_now_add_peer(&peerInfo)) {
    Serial.println("Added peer");
  }
  else {
    Serial.println("Failed to add peer");
    return false;
  }
  return true;
}


// Send a discovery packet to the broadcast address. Indicate the mode of the sender.
//
void BugComm::send_discovery() {
  if(MODE_UNINITIALIZED == device_mode) {
    Serial.println("ERROR: Call begin(mode)");
    return;
  }
  discovery.mode = device_mode;
  Serial.printf("Sending discovery message: %s\n", MODE_CONTROLLER == device_mode ? "MODE_CONTROLLER" : "MODE_RECEIVER");
  esp_now_send(broadcastAddress, (uint8_t*)&discovery, sizeof(BugComm_Discovery));
}


// Send a status response back to the BugController to let it know how the last message was handled.
//
void BugComm::send_response(BugComm_Status status) {
  response.status = status;
  esp_err_t result = esp_now_send(peerAddress, (uint8_t *) &response, sizeof(BugComm_Response));
#ifdef DEBUG_MSG_ON_DATA_SENT
  Serial.printf("send_response result = %d\n", result);
#endif
}


// If Y is < 0, trim the speed of motors 1 & 3
// If Y is > 0. trim the speed of motors 0 & 2
//
void BugComm::send_command(int8_t x, int8_t y, bool button) {
  // Joystick values are +/- 128, we need to scale these to +/- 100
  x =  (int8_t)(((int16_t)(x)*100)/128);
  y = -(int8_t)(((int16_t)(y)*100)/128);  // Invert y so that steering seems normal
  // dampen down the small numbers
  if (abs(x) < 2) x = 0;
  if (abs(y) < 2) y = 0;
  float delta = y / 100.0;
  if(0 > delta) delta = 1.0 + delta;
  else if(0 < delta) delta = -(1.0 - delta);
  if(last_x != x || last_y != y || last_b != button) {
    last_x = x;
    last_y = y;
    last_b = button;
    uint32_t color = 0x0000;            // Black
    if(x > 2) color = 0x001000;         // Moving forward, set color to green
    else if(y < -2) color = 0x100000;   // Moving backward, set color to red

    // If Y is < 0, trim the speed of motors 1 & 3
    // If Y is > 0. trim the speed of motors 0 & 2
    uint8_t scaled_value = (int)((float)x * delta);
    command.speed_0      = 0 > delta ?  x : scaled_value;
    command.speed_1      = 0 < delta ? -x : scaled_value;
    command.speed_2      = 0 > delta ?  x : scaled_value;
    command.speed_3      = 0 < delta ? -x : scaled_value;
    command.color_left   = color;
    command.color_right  = color;
    command.button       = button;
    esp_now_send(peerAddress, (uint8_t*)&command, sizeof(BugComm_Command));
  }
}


// When waiting for paring, process incoming discovery packet. If valid, remove broadcast peer
// and reinitialize with new peer. Mode is the mode of this station, not the peer.
//
void BugComm::process_discovery_response() {
  if(MODE_UNINITIALIZED == device_mode) {
    Serial.println("ERROR: Call begin(mode)");
    return;
  }
  if(data_ready) {
    Serial.printf("Processing discovery response. msg_kind = %d, msg_len = %d, mode = %d\n", msg_kind, response_len, discovery.mode);
    data_ready = false;
    data_valid = (KIND_DISCOVERY == msg_kind && sizeof(BugComm_Discovery) == response_len);
    if(data_valid) data_valid = BUGCOMM_SIGNATURE == discovery.signature &&
                                BUGCOMM_VERSION   == discovery.version &&
                                discovery.mode    == (MODE_CONTROLLER == device_mode) ? MODE_RECEIVER : MODE_CONTROLLER;
    if(data_valid) {
      Serial.println("Incoming discovery packet validated.");
      connected = true;
      send_discovery();
      memcpy(peerAddress, responseAddress, 6);              // This is who we will be talking to.
      if (ESP_OK == esp_now_del_peer(broadcastAddress)) {   // We are finished with discovery
        Serial.println("Deleted broadcast peer");
      }
      else {
        Serial.println("Failed to delete broadcast peer");
        return;
      }
      // Reinitialize WiFi with the new channel, which must match ESP-Now channel
      initialize_esp_now(channel, responseAddress);
      return;
    }
    else {
      Serial.print("COMM FAILURE: Incoming packet rejected. ");
      if(sizeof(BugComm_Discovery) != response_len) Serial.printf("Expected size: %d. Actual size: %d\n", sizeof(BugComm_Discovery), response_len);
      else if(BUGCOMM_SIGNATURE != response.signature) Serial.printf("Expected signature: %lu. Actual signature: %lu\n", BUGCOMM_SIGNATURE, response.signature);
      else if(BUGCOMM_VERSION   != response.version)   Serial.printf("Expected version: %d. Actual version: %d\n", BUGCOMM_VERSION, response.version);
      else if(discovery.mode    != (device_mode == MODE_CONTROLLER) ? MODE_RECEIVER : MODE_CONTROLLER) Serial.printf("Expected kind: %s. Actual kind: %s\n",
                                   (device_mode == MODE_CONTROLLER) ? "MODE_RECEIVER" : "MODE_CONTROLLER",
                                   (device_mode == MODE_CONTROLLER) ? "MODE_CONTROLLER" : "MODE_RECEIVER");
      else Serial.println("Coding error.\n");
    }
  }
}


// Static function: ESP-Now callback function that will be executed when data is sent
//
void BugComm::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#ifdef DEBUG_MSG_ON_DATA_SENT
  Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
#endif
}


// Static function: ESP-Now callback function that will be executed when data is received
// This is on a high-priority system thread. Do as little as possible.
// An incoming packet is arrainged in little-endian fashion and looks like this:
//    43 47 55 42 10 01 00 00 03 00 00 00 ...
//    |signature |ver        |kind       |data
//
void BugComm::on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
#ifdef DEBUG_DUMP_PACKET
  for(int i = 0; i < len; i++) { Serial.printf("%02X ", incomingData[i]); } Serial.println();
#endif
  if(sizeof(BugComm_Command) == len && KIND_COMMAND == ((uint32_t*)incomingData)[2]) {
    msg_kind = KIND_COMMAND;
    memcpy(&command, incomingData, sizeof(BugComm_Command));
    data_valid = BUGCOMM_SIGNATURE == command.signature &&
                 BUGCOMM_VERSION   == command.version;
    // Serial.printf("Incoming command message received: %s\n", data_valid ? "Valid" : "Invalid");
    if(data_valid) send_response(RESP_NOERR);
  }
  else if(sizeof(BugComm_Response) == len && KIND_RESPONSE == ((uint32_t*)incomingData)[2]) {
    msg_kind = KIND_RESPONSE;
    memcpy(&response, incomingData, sizeof(BugComm_Response));
    data_valid = (sizeof(BugComm_Response) == len);
    if(data_valid) data_valid = BUGCOMM_SIGNATURE == response.signature &&
                                BUGCOMM_VERSION   == response.version;
    // Serial.printf("Incoming response message received: %s\n", data_valid ? "Valid" : "Invalid");
  }
  else if(sizeof(BugComm_Discovery) == len && KIND_DISCOVERY == ((uint32_t*)incomingData)[2]) {
    msg_kind = KIND_DISCOVERY;
    memcpy(&discovery, incomingData, sizeof(BugComm_Discovery));
    data_valid = (sizeof(BugComm_Discovery) == len);
    if(data_valid) data_valid = BUGCOMM_SIGNATURE == discovery.signature &&
                                BUGCOMM_VERSION   == discovery.version;
    Serial.printf("Incoming discovery message received: %s\n", data_valid ? "Valid" : "Invalid");
  }
  else {
    Serial.printf("Incoming message of unknown kind received: %d\n", ((uint32_t*)incomingData)[2]);
  }
  memcpy(&responseAddress, mac, 6);
  response_len  = len;
  data_ready    = data_valid;
}


// Assumes (but does not test because you may have cleared it) that command data is valid.
//
uint32_t BugComm::get_light_color(uint8_t pos) {
  if(pos > 2) return 0;
  return (pos == 0) ? command.color_left : command.color_right;
}


// Assumes (but does not test because you may have cleared it) that command data is valid.
//
uint8_t BugComm::get_motor_speed(uint8_t pos) {
  switch(pos) {
    case 0:   return command.speed_0;
    case 1:   return command.speed_1;
    case 2:   return command.speed_2;
    case 3:   return command.speed_3;
    default:  return 0;
  }
}


// Assumes (but does not test because you may have cleared it) that command data is valid.
//
uint8_t BugComm::get_button() {
  return command.button;
}
