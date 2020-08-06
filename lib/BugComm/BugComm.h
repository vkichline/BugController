#pragma once
#include <stdint.h>
#include <esp_now.h>

// Note that the motors of the BugC are arrainged like:
//   1      3
//   0      2
// ...where left is the front of the BugC

#define BUGCOMM_SIGNATURE       0X42554743
#define BUGCOMM_VERSION         0X0200
#define BROADCAST_MAC_ADDRESS   {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define AP_NAME                 "BugNowAP"


enum BugComm_Status {
  RESP_NOERR,
  RESP_BUSY,
  RESP_ERROR
};


enum BugComm_Kind {
  KIND_NONE,
  KIND_COMMAND,
  KIND_RESPONSE,
  KIND_DISCOVERY
};


enum BugComm_Mode {
  MODE_CONTROLLER,
  MODE_RECEIVER,
  MODE_UNINITIALIZED
};


typedef struct BugComm_Command {
  ulong         signature = BUGCOMM_SIGNATURE;
  uint16_t      version   = BUGCOMM_VERSION;
  BugComm_Kind  kind      = KIND_COMMAND;
  int8_t        speed_0;
  int8_t        speed_1;
  int8_t        speed_2;
  int8_t        speed_3;
  uint32_t      color_left;
  uint32_t      color_right;
  bool          button;
} BugComm_Command;


typedef struct BugComm_Response {
  ulong           signature = BUGCOMM_SIGNATURE;
  uint16_t        version   = BUGCOMM_VERSION;
  BugComm_Kind    kind      = KIND_RESPONSE;
  BugComm_Status  status;
} BugComm_Response;


typedef struct BugComm_Discovery {
  ulong         signature = BUGCOMM_SIGNATURE;
  uint16_t      version   = BUGCOMM_VERSION;
  BugComm_Kind  kind      = KIND_DISCOVERY;
  BugComm_Mode  mode;
} BugComm_Discovery;


class BugComm {
  public:
    static void         begin(BugComm_Mode mode);   // Mode of this unit, not IF the peer
    static bool         initialize_esp_now(uint8_t chan, uint8_t* mac_address = broadcastAddress);
    static void         send_discovery();
    static void         send_response(BugComm_Status status);
    static void         send_command(int8_t x, int8_t y, bool button);  // this takes x & y as +/- 128
    static void         process_discovery_response();
    static bool         is_connected()      { return connected;   }
    static bool         is_data_ready()     { return data_ready;  }
    static void         clear_data_ready()  { data_ready = false; }
    static bool         get_data_valid()    { return data_valid;  }
    static uint8_t*     get_peer_address()  { return peerAddress; }
    static uint8_t      get_channel()       { return channel;     }
    static BugComm_Kind get_msg_kind()      { return msg_kind;    }

    static uint32_t     get_light_color(uint8_t pos);
    static uint8_t      get_motor_speed(uint8_t pos);
    static uint8_t      get_button();
  private:
    static esp_now_peer_info_t peerInfo;
    static BugComm_Command     command;
    static BugComm_Response    response;
    static BugComm_Discovery   discovery;
    static BugComm_Kind        msg_kind;
    static BugComm_Mode        device_mode;
    static bool                data_ready;
    static bool                data_valid;
    static bool                connected;
    static uint8_t             channel;       // 0 - 14.
    static uint8_t             response_len;
    static uint8_t             responseAddress[6];
    static uint8_t             broadcastAddress[];
    static uint8_t             peerAddress[6];
    static void on_data_sent(    const uint8_t *mac, esp_now_send_status_t status);
    static void on_data_received(const uint8_t *mac, const uint8_t *incomingData, int len);

    static int8_t              last_x;
    static int8_t              last_y;
    static bool                last_b;
};
