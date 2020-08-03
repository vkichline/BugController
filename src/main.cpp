#include <esp_now.h>
#include <WiFi.h>
#include "Wire.h"
#include "M5StickC.h"
#include "BugCommunications.h"

// The motors of the BugC are arrainged like:
//   1      3
//   0      2
// ...where left is the front of the BugC

// Ver 2: Automatic discovery. Secrets file is eliminated.
// Controller:
// When turned on, select a channel: 1 - 14. Default is random.
// Set a special callback function to handle the pairing
// Add a broadcast peer and broadcast a controller frame until ACK received.
// Change the callback routine to operational callback
// Remove broadcast peer.
// Add the responder as a peer.
// Go into controller mode.
// Obeyer:
// When turned on, select a channel: 1 - 14. Choose the same one as the Controller.
// Add a broadcast peer and listen for controller frame until detected. Send ACK.
// Remove broadcast peer.
// Go into obeyer mode.


#define JOY_ADDR    0x38
#define BG_COLOR    NAVY
#define FG_COLOR    LIGHTGREY
#define AP_NAME     "BugNowControlAP"

struct_message      datagram;
struct_response     response;
discovery_message   discovery;
esp_now_peer_info_t peerInfo;
bool                data_ready          = false;
uint8_t             response_len        = 0;
uint8_t             broadcastAddress[]  = BROADCAST_MAC_ADDRESS;
uint8_t             responseAddress[6]  = { 0 };
uint8_t             bugAddress[6]       = { 0 };
uint8_t             channel             = 0;
bool                connected           = false;
control_mode        mode                = MODE_FORWARD;
int8_t              lastX               = 127;
int8_t              lastY               = 127;
bool                lastB               = false;
int8_t              JoyX                = 0;
int8_t              JoyY                = 0;
bool                JoyB                = false;
bool                debug_data          = false;          // Extra info about joystick and speed
bool                debug_send          = false;          // Extra info about ESP-Now communications


// Display the mac address on the screen in a diagnostic color
//
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugNow Controller", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  mac = String("C ") + mac;
  M5.Lcd.drawCentreString(mac, 80, 22, 2);
  if(connected) {
    char  buffer[32];
    sprintf(buffer, "R %02X %02X %02X %02X %02X %02X", bugAddress[0], bugAddress[1],
        bugAddress[2], bugAddress[3], bugAddress[4], bugAddress[5]);
    M5.Lcd.drawCentreString(buffer, 80, 40, 2);
    String chan = "Chan " + String(channel);
    M5.Lcd.drawCentreString(chan, 80, 60, 2);
  }
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


// Callback when data is sent
//
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if(debug_send) Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// Callback function that will be executed when data is received
// This is on a high-priority system thread. Do as little as possible.
//
void on_data_received(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if(debug_send) Serial.println("Incoming packet received.");
  memcpy(&response, incomingData, sizeof(struct_response));
  memcpy(&responseAddress, mac, 6);
  response_len  = len;
  data_ready    = true;
}


// Select and return the channel that we're going to use for communications. This enables racing, etc.
//
uint8_t select_comm_channel() {
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


// Set up ESP-Now. Return true if successful.
//
bool initialize_esp_now(uint8_t chan, uint8_t* mac_address) {
  channel = chan;
  WiFi.disconnect();
  bool restart = WiFi.softAP(AP_NAME, "", channel);
  WiFi.mode(WIFI_STA);
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


void process_pairing_response() {
  if(sizeof(struct_response) == response_len) {
    if(COMMUNICATIONS_SIGNATURE == response.signature && COMMUNICATIONS_VERSION == response.version) {
      Serial.println("Incoming packet validated.");
      if(!connected) {
        Serial.println("Connection packet");
        connected = true;
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x, channel %d\n", responseAddress[0], responseAddress[1], responseAddress[2], responseAddress[3], responseAddress[4], responseAddress[5], channel);
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
    Serial.printf("COMM FAILURE: Incoming packet rejected. Expected signature: %lu. Actual signature: %lu\n    Expected version: %d. Actual version: %d\n",
      COMMUNICATIONS_SIGNATURE, response.signature, COMMUNICATIONS_VERSION, response.version);
    }
  }
  else {
    Serial.printf("COMM FAILURE: Incoming packet rejected. Expected size: %d. Actual size: %d\n", response_len, sizeof(struct_response));
  }
}


// Broadcast a discovery_message until someone responds with a valid packet
//
bool pair_with_obeyer() {
  M5.Lcd.fillScreen(BG_COLOR);
  M5.Lcd.drawCentreString("Waiting for Pairing", 80, 20, 2);
  M5.Lcd.drawCentreString("on channel " + String(channel), 80, 40, 2);
  while(!connected) {
    esp_now_send(broadcastAddress, (uint8_t*)&discovery, sizeof(discovery_message));
    delay(500);
    if(data_ready) {
      data_ready = false;
      process_pairing_response();
    }
  }
  return true;
}


void setup() {
  M5.begin();
  Wire.begin(0, 26, 100000);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.fillScreen(BG_COLOR);
  channel = select_comm_channel();
  initialize_esp_now(channel, broadcastAddress);
  pair_with_obeyer();
  M5.Lcd.fillScreen(BLACK);
  print_mac_address(TFT_GREEN);
}


void loop() {
  m5.update();
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
    // M5.Lcd.setTextColor((0 <= JoyX) ? TFT_GREEN : TFT_RED);
    // M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
    // M5.Lcd.drawCentreString(String(JoyX)+"/"+String(JoyY), 80, 64, 2);
  }
  delay(100);
}
