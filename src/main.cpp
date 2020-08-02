#include <esp_now.h>
#include <WiFi.h>
#include "Wire.h"
#include "M5StickC.h"
#include "BugCommunications.h"
// M5StickCMacAddresses.h includes some defines, including a line like:
// #define of M5STICKC_MAC_ADDRESS_BUGC_ROBOT   {0xNN, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN}
// Replace the N's with the digits of your M5StickC's MAC Address (displayed on screen)
// NEVER check your secrets into a source control manager!
#include "../../Secrets/M5StickCMacAddresses.h"

// The motors of the BugC are arrainged like:
//   1      3
//   0      2
// ...where left is the front of the BugC

#define JOY_ADDR 0x38

struct_message  datagram;
struct_response response;
uint8_t         bugAddress[]        = M5STICKC_MAC_ADDRESS_BUGC_ROBOT;    // Specific to the BugC M5StickC
bool            connected           = false;
control_mode    mode                = MODE_FORWARD;
int8_t          lastX               = 127;
int8_t          lastY               = 127;
bool            lastB               = false;
int8_t          JoyX                = 0;
int8_t          JoyY                = 0;
bool            JoyB                = false;
bool            debug               = false;  // Extra serial spew if true
bool            debug_send          = false;


bool command_stop() {
  datagram.speed_0      = 0;
  datagram.speed_1      = 0;
  datagram.speed_2      = 0;
  datagram.speed_3      = 0;
  datagram.color_left   = BLACK;
  datagram.color_right  = BLACK;
  esp_err_t result = esp_now_send(bugAddress, (uint8_t*)&datagram, sizeof(struct_message));
  return (ESP_OK == result);
}


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
        connected = true;
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


void setup() {
  M5.begin();
  Wire.begin(0, 26, 100000);
  M5.Lcd.setRotation(1);
  print_mac_address(TFT_RED);
  esp_now_peer_info_t peerInfo;
  WiFi.mode(WIFI_STA);
  if(ESP_OK != esp_now_init()) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  memcpy(peerInfo.peer_addr, bugAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
    if (ESP_OK != esp_now_add_peer(&peerInfo)) {
    Serial.println("Failed to add peer");
    return;
  }
}


void loop() {
  m5.update();
  if(!connected) {
    command_stop();
    return;
  }
  read_joystick();
  if (debug) Serial.printf("X = %3d, Y = %3d, B = %s\t", JoyX, JoyY, JoyB ? " true" : "false");

  // Joystick values are +/- 128, we need to scale these to +/- 100
  JoyX = (int8_t)(((int16_t)(JoyX)*100)/128);
  JoyY = (int8_t)(((int16_t)(JoyY)*100)/128);
  // dampen down the small numbers
  if (abs(JoyX) < 2) JoyX = 0;
  if (abs(JoyY) < 2) JoyY = 0;
  float delta = JoyY / 100.0;
  if(0 > delta) delta = 1.0 + delta;
  else if(0 < delta) delta = -(1.0 - delta);
  if (debug) Serial.printf("ScaledX = %3d, ScaledY = %3d, delta = %.3f\n", JoyX, JoyY, delta);
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
