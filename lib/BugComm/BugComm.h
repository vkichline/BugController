#pragma once
#include <NowComm.h>

// Just a test of the NowComm Template Class

typedef struct BugCommand {
  ulong         signature = NOWCOMM_SIGNATURE;
  uint16_t      version   = NOWCOMM_VERSION;
  NowComm_Kind  kind      = NOWCOMM_KIND_COMMAND;
  int8_t        speed_0;
  int8_t        speed_1;
  int8_t        speed_2;
  int8_t        speed_3;
  uint32_t      color_left;
  uint32_t      color_right;
  bool          button;
} BugCommand;


class BugComm : public NowComm<BugCommand> {
  public:
    void        send_command(int8_t x, int8_t y, bool button);  // this takes x & y as +/- 128
    uint32_t    get_light_color(uint8_t pos);
    uint8_t     get_motor_speed(uint8_t pos);
    uint8_t     get_button();
  private:
    int8_t      last_x  = 127;
    int8_t      last_y  = 127;
    bool        last_b  = false;
};
