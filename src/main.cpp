// BugNowController Robot controller with an M5StickC using ESP-Now protocol
// Pairs with the companion project BugNow
// By Van Kichline
// In the year of the plague
//
// Ver 2: Automatic discovery. Secrets file is eliminated.
// Controller:
// When turned on, select a channel: 1 - 14. Default is random.
// Add a broadcast peer and broadcast a discovery packet until ACK received.
// Remove broadcast peer.
// Add the responder as a peer.
// Go into controller mode.
// Receiver:
// When turned on, select a channel: 1 - 14. Choose the same one as the Controller.
// Add a broadcast peer and listen for discovery packet until detected. Send ACK.
// Remove broadcast peer.
// Go into receiver mode.


#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5StickC.h>
#include <BugComm.h>

#define JOY_ADDR    0x38
#define BG_COLOR    NAVY
#define FG_COLOR    LIGHTGREY


BugComm             bug_comm;                             // From NowComm template class
bool                comp_mode           = false;          // Competition mode: manually select a channel
int8_t              JoyX                = 0;
int8_t              JoyY                = 0;
bool                JoyB                = false;


// Display the mac address of the device, and if connected, of its paired device.
// Also show the channel in use.
// Red indicates no ESP-Now connection, Green indicates connection established.
//
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugNow Controller", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  mac = String("C ") + mac;
  M5.Lcd.drawCentreString(mac, 80, 22, 2);
  String chan = "Chan " + String(bug_comm.get_channel());
  M5.Lcd.drawCentreString(chan, 80, 60, 2);
  if(bug_comm.is_connected()) {
    char  buffer[32];
    uint8_t*  pa = bug_comm.get_peer_address();
    sprintf(buffer, "R %02X %02X %02X %02X %02X %02X", pa[0], pa[1], pa[2], pa[3], pa[4], pa[5]);
    M5.Lcd.drawCentreString(buffer, 80, 40, 2);
  }
}


// If we're not in competition mode, simply return 1.
// Else, select and return the channel that we're going to use for communications. This enables racing, etc.
//
uint8_t select_comm_channel() {
  if(!comp_mode) return 1;

  uint8_t chan  = random(13) + 1;
  M5.Lcd.drawString("CHAN",    8,  4, 2);
  M5.Lcd.drawString("1 - 14",  8, 28, 1);
  M5.Lcd.drawString("A = +",   8, 46, 1);
  M5.Lcd.drawString("B = Set", 8, 64, 1);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(String(chan), 160, 2, 8);

  // Before transmitting, select a channel
  while(true) {
    M5.update();
    if(M5.BtnB.wasReleased()) break;  // EXIT THE LOOP BY PRESSING B
    if(M5.BtnA.wasReleased()) {
      chan++;
      if(14 < chan) {
        chan = 1;
        M5.Lcd.fillRect(60, 2, 100, 80, BG_COLOR);
      }
      M5.Lcd.drawString(String(chan), 160, 2, 8);
    }
  }
  M5.Lcd.setTextDatum(TL_DATUM);
  return chan;
}


// Put X, Y and Button values in globals JoyX, JoyB, JoyB
//
void read_joystick() {
  Wire.beginTransmission(JOY_ADDR);
  Wire.write(0x02);
  Wire.endTransmission();
  Wire.requestFrom(JOY_ADDR, 3);
  if (Wire.available()) {
    JoyX = Wire.read();
    JoyY = Wire.read();
    JoyB = Wire.read() > 0 ? false : true;  // Button value is inverted
  }
}


// Broadcast a BugComm_Discovery until someone responds with a valid packet
//
bool pair_with_receiver() {
  if(comp_mode) {
    M5.Lcd.fillScreen(BG_COLOR);
    M5.Lcd.drawCentreString("Waiting for Pairing", 80, 20, 2);
    M5.Lcd.drawCentreString("on channel " + String(bug_comm.get_channel()), 80, 40, 2);
  }
  else {
    M5.Lcd.fillScreen(TFT_BLACK);
    print_mac_address(TFT_RED);
  }
  while(!bug_comm.is_connected()) {
    bug_comm.send_discovery();
    delay(500);
    if(bug_comm.is_data_ready() && NOWCOMM_KIND_DISCOVERY == bug_comm.get_msg_kind()) {
      Serial.println("Discovery response received.");
      bug_comm.process_discovery_response();
      print_mac_address(TFT_GREEN);
    }
  }
  return true;
}


void process_joystick() {
  read_joystick();
  bug_comm.send_command(JoyX, JoyY, JoyB);
}


void display_battery_voltage() {
  M5.Lcd.setTextColor((0 <= JoyX) ? TFT_GREEN : TFT_RED);
  M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
  M5.Lcd.drawCentreString(String(JoyX)+"/"+String(JoyY), 80, 64, 2);
}


void setup() {
  if(digitalRead(BUTTON_A_PIN) == 0) {
    comp_mode = true;
  }
  M5.begin();
  Wire.begin(0, 26, 100000);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.fillScreen(BG_COLOR);

  bug_comm.begin(NOWCOMM_MODE_CONTROLLER, select_comm_channel());
  pair_with_receiver();
  M5.Lcd.fillScreen(BLACK);
  print_mac_address(TFT_GREEN);
}


void loop() {
  m5.update();
  process_joystick();
  delay(100);
}
