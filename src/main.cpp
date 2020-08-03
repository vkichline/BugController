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


#define JOY_ADDR  0x38
#define BG_COLOR  NAVY
#define FG_COLOR  LIGHTGREY

struct_message  datagram;
struct_response response;
uint8_t         broadcastAddress[]  = BROADCAST_MAC_ADDRESS;
uint8_t         bugAddress[6]       = { 0 };
uint8_t         channel             = 0;
bool            connected           = false;
control_mode    mode                = MODE_FORWARD;
int8_t          lastX               = 127;
int8_t          lastY               = 127;
bool            lastB               = false;
int8_t          JoyX                = 0;
int8_t          JoyY                = 0;
bool            JoyB                = false;
bool            debug_data          = false;                    // Extra info about joystick and speed
bool            debug_send          = false;                    // Extra info about ESP-Now communications


// display the mac address on the screen in a diagnostic color
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color);
  M5.Lcd.drawCentreString("BugC Controller", 80, 0, 2);
  String mac = WiFi.macAddress();
  mac.replace(":", " ");
  M5.Lcd.drawCentreString(mac, 80, 30, 2);
}


// put X, Y and Button values in globals JoyX, JoyB, JoyB
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


// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if(debug_send) Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.println("Incoming packet received.");
  if(sizeof(struct_response) == len) {
    struct_response* resp = (struct_response*)incomingData;
    if(COMMUNICATIONS_SIGNATURE == resp->signature && COMMUNICATIONS_VERSION == resp->version) {
      Serial.println("Incoming packet validated.");
      if(!connected) {
        esp_now_peer_info peerInfo;
        connected = true;
        esp_now_del_peer(broadcastAddress);   // We are finished with discovery
        memcpy(bugAddress, mac, 6);           // This is who we will be talking to.
        memcpy(peerInfo.peer_addr, mac, 6);   // Register as a peer
        peerInfo.channel = channel;
        peerInfo.encrypt = false;
          if (ESP_OK != esp_now_add_peer(&peerInfo)) {
          Serial.println("Failed to add peer");
          return;
        }

        print_mac_address(TFT_GREEN);
      }
    }
    else {
    Serial.printf("COMM FAILURE: Incoming packet rejected. Expected signature: %lu. Actual signature: %lu\n    Expected version: %d. Actual version: %d\n",
      COMMUNICATIONS_SIGNATURE, resp->signature, COMMUNICATIONS_VERSION, resp->version);
    }
  }
  else {
    Serial.printf("COMM FAILURE: Incoming packet rejected. Expected size: %d. Actual size: %d\n", len, sizeof(struct_response));
  }
}


// Select and return the channel that we're going to use for communications. This enables racing, etc.
//
uint8_t select_comm_channel() {
  uint8_t chan  = random(13) + 1;
  M5.Lcd.drawString("Channel", 8,  8, 1);
  M5.Lcd.drawString("1 - 14",  8, 26, 1);
  M5.Lcd.drawString("A = +",   8, 44, 1);
  M5.Lcd.drawString("B = set", 8, 62, 1);
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


// Broadcast a discovery_message until someone responds with a valid packet
//
void pair_with_obeyer() {
  esp_now_peer_info peerInfo;
  discovery_message msg;

  M5.Lcd.fillScreen(BG_COLOR);
  M5.Lcd.drawCentreString("Waiting for Pairing", 80,  8, 2);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
    if (ESP_OK != esp_now_add_peer(&peerInfo)) {
    Serial.println("Failed to add peer");
    return;
  }
  while(true) {
    esp_now_send(peerInfo.peer_addr, (uint8_t*)&msg, sizeof(discovery_message));
    delay(500);
    if(connected) break;
  }
}


void setup() {
  M5.begin();
  Wire.begin(0, 26, 100000);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.fillScreen(BG_COLOR);
  channel = select_comm_channel();
  WiFi.mode(WIFI_STA);
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  pair_with_obeyer();
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
    M5.Lcd.setTextColor((0 <= JoyX) ? TFT_GREEN : TFT_RED);
    M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
    M5.Lcd.drawCentreString(String(JoyX)+"/"+String(JoyY), 80, 64, 2);
  }
  delay(100);
}
