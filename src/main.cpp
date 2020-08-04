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
#include "Wire.h"
#include "M5StickC.h"
#include "BugCommunications.h"

#define JOY_ADDR    0x38
#define BG_COLOR    NAVY
#define FG_COLOR    LIGHTGREY
#define AP_NAME     "BugNowControlAP"

struct_message      datagram;
struct_response     response;
struct_discovery   discovery;
esp_now_peer_info_t peerInfo;
bool                comp_mode           = false;          // Competition mode: manually select a channel
bool                data_ready          = false;
uint8_t             response_len        = 0;
uint8_t             broadcastAddress[]  = BROADCAST_MAC_ADDRESS;
uint8_t             responseAddress[6]  = { 0 };
uint8_t             bugAddress[6]       = { 0 };
uint8_t             channel             = 0;
bool                connected           = false;
int8_t              JoyX                = 0;
int8_t              JoyY                = 0;
bool                JoyB                = false;
int8_t              lastX               = 127;
int8_t              lastY               = 127;
bool                lastB               = false;
bool                debug_data          = false;          // Extra info about joystick and speed
bool                debug_send          = false;          // Extra info about ESP-Now communications


// ESP-Now callback function that will be executed when data is sent
//
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if(debug_send) Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// ESP-Now callback function that will be executed when data is received
// This is on a high-priority system thread. Do as little as possible.
//
void on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if(debug_send) Serial.println("Incoming packet received.");
  memcpy(&response, incomingData, sizeof(struct_response));
  memcpy(&responseAddress, mac, 6);
  response_len  = len;
  data_ready    = true;
}


// Set up ESP-Now. Return true if successful.
//
bool initialize_esp_now(uint8_t chan, uint8_t* mac_address) {
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

  memcpy(peerInfo.peer_addr, mac_address, 6);
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
  String chan = "Chan " + String(channel);
  M5.Lcd.drawCentreString(chan, 80, 60, 2);
  if(connected) {
    char  buffer[32];
    sprintf(buffer, "R %02X %02X %02X %02X %02X %02X", bugAddress[0], bugAddress[1],
        bugAddress[2], bugAddress[3], bugAddress[4], bugAddress[5]);
    M5.Lcd.drawCentreString(buffer, 80, 40, 2);
  }
}


// When waiting for paring, process incoming response packet. If valid, remove broadcast peer
// and reinitialize with new peer.
//
void process_pairing_response() {
  if(response_len == comp_mode ? sizeof(struct_response) : sizeof(struct_discovery)) {
    if(COMMUNICATIONS_SIGNATURE == response.signature && COMMUNICATIONS_VERSION == response.version) {
      Serial.println("Incoming response packet validated.");
      if(!connected) {
        connected = true;
        memcpy(bugAddress, responseAddress, 6);         // This is who we will be talking to.

        if (ESP_OK == esp_now_del_peer(broadcastAddress)) {   // We are finished with discovery
          Serial.println("Deleted broadcast peer");
        }
        else {
          Serial.println("Failed to delete broadcast peer");
        }
        // Reinitialize WiFi with the new channel, which must match ESP-Now channel
        initialize_esp_now(channel, responseAddress);
        print_mac_address(TFT_GREEN);
      }
    }
    else {
      Serial.printf("COMM FAILURE: Incoming packet rejected. Expected signature: %lu. Actual signature: %lu\n", COMMUNICATIONS_SIGNATURE, response.signature);
      Serial.printf("              Expected version: %d. Actual version: %d\n", COMMUNICATIONS_VERSION, response.version);
    }
  }
  else {
    Serial.printf("COMM FAILURE: Incoming packet rejected. Expected size: %d. Actual size: %d\n", sizeof(struct_response), response_len);
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


// Broadcast a struct_discovery until someone responds with a valid packet
//
bool pair_with_receiver() {
  if(comp_mode) {
    M5.Lcd.fillScreen(BG_COLOR);
    M5.Lcd.drawCentreString("Waiting for Pairing", 80, 20, 2);
    M5.Lcd.drawCentreString("on channel " + String(channel), 80, 40, 2);
  }
  else {
    M5.Lcd.fillScreen(TFT_BLACK);
    print_mac_address(TFT_RED);
  }
  while(!connected) {
    esp_now_send(broadcastAddress, (uint8_t*)&discovery, sizeof(struct_discovery));
    delay(500);
    if(data_ready) {
      data_ready = false;
      process_pairing_response();
    }
  }
  return true;
}


void process_joystick() {
  read_joystick();
  if (debug_data) Serial.printf("X = %3d, Y = %3d, B = %s\t", JoyX, JoyY, JoyB ? " true" : "false");

  // Joystick values are +/- 128, we need to scale these to +/- 100
  JoyX = (int8_t)(((int16_t)(JoyX)*100)/128);
  JoyY = (int8_t)(((int16_t)(JoyY)*100)/128);
  // dampen down the small numbers
  if (abs(JoyX) < 2) JoyX = 0;
  if (abs(JoyY) < 2) JoyY = 0;
  float delta = JoyY / 100.0;
  if(0 > delta) delta = 1.0 + delta;
  else if(0 < delta) delta = -(1.0 - delta);
  if (debug_data) Serial.printf("ScaledX = %3d, ScaledY = %3d, delta = %.3f\n", JoyX, JoyY, delta);
  if(lastX != JoyX || lastY != JoyY || lastB != JoyB) {
    lastX = JoyX;
    lastY = JoyY;
    lastB = JoyB;
    uint32_t color = TFT_BLACK;
    if(JoyX > 2) color = 0x001000;        // Moving forward, set color to green
    else if(JoyX < -2) color = 0x100000;  // Moving backward, set color to red

    // If Y is < 0, trim the speed of motors 1 & 3
    // If Y is > 0. trim the speed of motors 0 & 2
    uint8_t scaled_value = (int)((float)JoyX * delta);
    datagram.speed_0      = 0 > delta ?  JoyX : scaled_value;
    datagram.speed_1      = 0 < delta ? -JoyX : scaled_value;
    datagram.speed_2      = 0 > delta ?  JoyX : scaled_value;
    datagram.speed_3      = 0 < delta ? -JoyX : scaled_value;
    datagram.color_left   = color;
    datagram.color_right  = color;
    datagram.button       = JoyB;
    esp_now_send(bugAddress, (uint8_t*)&datagram, sizeof(struct_message));
  }
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

  channel = select_comm_channel();
  initialize_esp_now(channel, broadcastAddress);
  pair_with_receiver();
  M5.Lcd.fillScreen(BLACK);
  print_mac_address(TFT_GREEN);
}


void loop() {
  m5.update();
  process_joystick();
  // display_battery_voltage();
  delay(100);
}
