#pragma once
#include <stdint.h>
#include <esp_now.h>
#include <WiFi.h>

#define NOWCOMM_SIGNATURE       0x43574F4E
#define NOWCOMM_VERSION         0X0211
#define BROADCAST_MAC_ADDRESS   {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define NOWCOMM_AP_NAME         "NowCommAP"

// #define DEBUG_MSG_ON_DATA_SENT
// #define DEBUG_DUMP_PACKET


enum NowComm_Status {
  NOWCOMM_RESP_NOERR,
  NOWCOMM_RESP_BUSY,
  NOWCOMM_RESP_ERROR
};


enum NowComm_Kind {
  NOWCOMM_KIND_NONE,
  NOWCOMM_KIND_COMMAND,
  NOWCOMM_KIND_RESPONSE,
  NOWCOMM_KIND_DISCOVERY
};


enum NowComm_Mode {
  NOWCOMM_MODE_CONTROLLER,
  NOWCOMM_MODE_RECEIVER,
  NOWCOMM_MODE_UNINITIALIZED
};


typedef struct NowComm_Response {
  ulong           signature = NOWCOMM_SIGNATURE;
  uint16_t        version   = NOWCOMM_VERSION;
  NowComm_Kind    kind      = NOWCOMM_KIND_RESPONSE;
  NowComm_Status  status;
} NowComm_Response;


typedef struct NowComm_Discovery {
  ulong         signature = NOWCOMM_SIGNATURE;
  uint16_t      version   = NOWCOMM_VERSION;
  NowComm_Kind  kind      = NOWCOMM_KIND_DISCOVERY;
  NowComm_Mode  mode;
} NowComm_Discovery;


// T is the type of the structure used for sending commands.
// It must be no more than 250 bytes total, and the first three fields must be defined is:
//   ulong         signature = NOWCOMM_SIGNATURE;
//   uint16_t      version   = NOWCOMM_VERSION;
//   NowComm_Kind  kind      = NOWCOMM_KIND_COMMAND;
//
template <class T>
class NowComm {
  public:
    void                 begin(NowComm_Mode mode, uint8_t chan);   // Mode of this unit, not the peer.  Channel = 1 - 14
    void                 send_discovery();
    void                 process_discovery_response();
    void                 send_command(T* command);
    void                 send_response(NowComm_Status status);
    bool                 is_connected()      { return connected;   }
    bool                 is_data_ready()     { return data_ready;  }
    void                 clear_data_ready()  { data_ready = false; }
    bool                 get_data_valid()    { return data_valid;  }
    uint8_t*             get_peer_address()  { return peerAddress; }
    uint8_t              get_channel()       { return channel;     }
    NowComm_Kind         get_msg_kind()      { return msg_kind;    }
    T*                   get_data()          { return &command;    }
  protected:
    T                    command;
  private:
    static NowComm<T>*   self_reference;      // TODO: I'm not happy with this solution
    static void          on_data_sent_wrapper(const uint8_t *mac, esp_now_send_status_t status);
    static void          on_data_received_wrapper(const uint8_t *mac, const uint8_t *incomingData, int len);
    bool                 initialize_esp_now(uint8_t chan, uint8_t* mac_address);
    void                 on_data_sent(const uint8_t *mac, esp_now_send_status_t status);
    void                 on_data_received(const uint8_t *mac, const uint8_t *incomingData, int len);
    esp_now_peer_info_t  peerInfo;
    NowComm_Response     response;
    NowComm_Discovery    discovery;
    NowComm_Kind         msg_kind             = NOWCOMM_KIND_NONE;
    NowComm_Mode         device_mode          = NOWCOMM_MODE_UNINITIALIZED;
    bool                 data_ready           = false;
    bool                 data_valid           = false;
    bool                 connected            = false;
    uint8_t              channel              = 0;
    uint8_t              response_len         = 0;
    uint8_t              responseAddress[6]   = { 0 };
    uint8_t              broadcastAddress[6]  = BROADCAST_MAC_ADDRESS;
    uint8_t              peerAddress[6]       = { 0 };
};

template <class T> NowComm<T>* NowComm<T>::self_reference;


// Set the mode that this device operates in.
//
template <typename T> void NowComm<T>::begin(NowComm_Mode mode, uint8_t chan) {
  device_mode = mode;
  channel     = chan;
  initialize_esp_now(mode, broadcastAddress);
}


// Make sure wifi is running bug idle by starting hidden access point and switching to station mode.
// Set WiFi channel to channel provided.
// Register callbacks and set the peer address.
// TODO: Handle startup w/ WiFi already running, switch channels w/ autoconnect.
//
template <typename T> bool NowComm<T>::initialize_esp_now(uint8_t chan, uint8_t* mac) {
  WiFi.disconnect();
  WiFi.softAP(NOWCOMM_AP_NAME, "", channel, 1); // Create a hidden AP on given channel
  WiFi.mode(WIFI_STA);                          // ...and switch to station mode
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  self_reference = this;       // TODO: I'm not happy with this solution
  esp_now_register_send_cb(on_data_sent_wrapper);
  esp_now_register_recv_cb(on_data_received_wrapper);

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
template <typename T> void NowComm<T>::send_discovery() {
  if(NOWCOMM_MODE_UNINITIALIZED == device_mode) {
    Serial.println("ERROR: Call begin(mode)");
    return;
  }
  discovery.mode = device_mode;
  Serial.printf("Sending discovery message: %s\n", NOWCOMM_MODE_CONTROLLER == device_mode ? "NOWCOMM_MODE_CONTROLLER" : "NOWCOMM_MODE_RECEIVER");
  esp_now_send(broadcastAddress, (uint8_t*)&discovery, sizeof(NowComm_Discovery));
}


// Send a status response back to the BugController to let it know how the last message was handled.
//
template <typename T> void NowComm<T>::send_response(NowComm_Status status) {
  response.status = status;
#ifdef DEBUG_MSG_ON_DATA_SENT
  esp_err_t result = esp_now_send(peerAddress, (uint8_t *) &response, sizeof(NowComm_Response));
  Serial.printf("send_response result = %d\n", result);
#else
  esp_now_send(peerAddress, (uint8_t *) &response, sizeof(NowComm_Response));
#endif
#ifdef DEBUG_DUMP_PACKET
  for(int i = 0; i < 6; i++) { Serial.printf("%02X", peerAddress[i]); } Serial.print(" Out ");
  for(int i = 0; i < sizeof(NowComm_Response); i++) { Serial.printf("%02X ", ((char*)&response)[i]); } Serial.println();
#endif
}


// Send the data structure the template was created with
//
template <typename T> void NowComm<T>::send_command(T* data) {
#ifdef DEBUG_DUMP_PACKET
  for(int i = 0; i < 6; i++) { Serial.printf("%02X", peerAddress[i]); } Serial.print(" Out ");
  for(int i = 0; i < sizeof(T); i++) { Serial.printf("%02X ", ((char*)data)[i]); } Serial.println();
#endif
  esp_now_send(peerAddress, (uint8_t*)data, sizeof(T));
}


// When waiting for paring, process incoming discovery packet. If valid, remove broadcast peer
// and reinitialize with new peer. Mode is the mode of this station, not the peer.
//
template <typename T> void NowComm<T>::process_discovery_response() {
  if(NOWCOMM_MODE_UNINITIALIZED == device_mode) {
    Serial.println("ERROR: Call begin(mode), channel");
    return;
  }
  if(data_ready) {
    Serial.printf("Processing discovery response. msg_kind = %d, msg_len = %d, mode = %d\n", msg_kind, response_len, discovery.mode);
    data_ready = false;
    data_valid = (NOWCOMM_KIND_DISCOVERY == msg_kind && sizeof(NowComm_Discovery) == response_len);
    if(data_valid) data_valid = NOWCOMM_SIGNATURE == discovery.signature &&
                                NOWCOMM_VERSION   == discovery.version &&
                                discovery.mode    == (NOWCOMM_MODE_CONTROLLER == device_mode) ? NOWCOMM_MODE_RECEIVER : NOWCOMM_MODE_CONTROLLER;
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
      if(sizeof(NowComm_Discovery) != response_len) Serial.printf("Expected size: %d. Actual size: %d\n", sizeof(NowComm_Discovery), response_len);
      else if(NOWCOMM_SIGNATURE != response.signature) Serial.printf("Expected signature: %lu. Actual signature: %lu\n", NOWCOMM_SIGNATURE, response.signature);
      else if(NOWCOMM_VERSION   != response.version)   Serial.printf("Expected version: %d. Actual version: %d\n", NOWCOMM_VERSION, response.version);
      else if(discovery.mode    != (device_mode == NOWCOMM_MODE_CONTROLLER) ? NOWCOMM_MODE_RECEIVER : NOWCOMM_MODE_CONTROLLER) Serial.printf("Expected kind: %s. Actual kind: %s\n",
                                   (device_mode == NOWCOMM_MODE_CONTROLLER) ? "NOWCOMM_MODE_RECEIVER" : "NOWCOMM_MODE_CONTROLLER",
                                   (device_mode == NOWCOMM_MODE_CONTROLLER) ? "NOWCOMM_MODE_CONTROLLER" : "NOWCOMM_MODE_RECEIVER");
      else Serial.println("Coding error.\n");
    }
  }
}


// Static function: ESP-Now callback function that will be executed when data is sent
//
template <typename T> void NowComm<T>::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#ifdef DEBUG_MSG_ON_DATA_SENT
  Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
#endif
}


// ESP-Now callback function that will be executed when data is received
// This is on a high-priority system thread. Do as little as possible.
// An incoming packet is arrainged in little-endian fashion and looks like this:
//    43 47 55 42 10 01 00 00 03 00 00 00 ...
//    |signature |ver        |kind       |data
//
template <typename T> void NowComm<T>::on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
#ifdef DEBUG_DUMP_PACKET
  for(int i = 0; i < 6; i++) { Serial.printf("%02X", mac[i]); } Serial.print(" In  ");
  for(int i = 0; i < len; i++) { Serial.printf("%02X ", incomingData[i]); } Serial.println();
#endif
  if(sizeof(T) == len && NOWCOMM_KIND_COMMAND == ((uint32_t*)incomingData)[2]) {
    msg_kind = NOWCOMM_KIND_COMMAND;
    memcpy(&command, incomingData, sizeof(T));
    data_valid = NOWCOMM_SIGNATURE == command.signature &&
                 NOWCOMM_VERSION   == command.version;
    // Serial.printf("Incoming command message received: %s\n", data_valid ? "Valid" : "Invalid");
    if(data_valid) send_response(NOWCOMM_RESP_NOERR);
  }
  else if(sizeof(NowComm_Response) == len && NOWCOMM_KIND_RESPONSE == ((uint32_t*)incomingData)[2]) {
    msg_kind = NOWCOMM_KIND_RESPONSE;
    memcpy(&response, incomingData, sizeof(NowComm_Response));
    data_valid = (sizeof(NowComm_Response) == len);
    if(data_valid) data_valid = NOWCOMM_SIGNATURE == response.signature &&
                                NOWCOMM_VERSION   == response.version;
    // Serial.printf("Incoming response message received: %s\n", data_valid ? "Valid" : "Invalid");
  }
  else if(sizeof(NowComm_Discovery) == len && NOWCOMM_KIND_DISCOVERY == ((uint32_t*)incomingData)[2]) {
    msg_kind = NOWCOMM_KIND_DISCOVERY;
    memcpy(&discovery, incomingData, sizeof(NowComm_Discovery));
    data_valid = (sizeof(NowComm_Discovery) == len);
    if(data_valid) data_valid = NOWCOMM_SIGNATURE == discovery.signature &&
                                NOWCOMM_VERSION   == discovery.version;
    Serial.printf("Incoming discovery message received: %s\n", data_valid ? "Valid" : "Invalid");
  }
  else {
    Serial.printf("Incoming message of unknown kind received: %d\n", ((uint32_t*)incomingData)[2]);
  }
  memcpy(&responseAddress, mac, 6);
  response_len  = len;
  data_ready    = data_valid;
}


template <typename T> void NowComm<T>::on_data_sent_wrapper(const uint8_t *mac, esp_now_send_status_t status) {
  auto func = std::bind(&NowComm<T>::on_data_sent, self_reference, mac, status);
  func();
}


template <typename T> void NowComm<T>::on_data_received_wrapper(const uint8_t *mac, const uint8_t *incomingData, int len) {
  auto func = std::bind(&NowComm<T>::on_data_received, self_reference, mac, incomingData, len);
  func();
}
