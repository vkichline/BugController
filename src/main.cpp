#include <esp_now.h>
#include <WiFi.h>
#include "M5StickC.h"
#include "BugCommunications.h"
// M5StickCMacAddresses.h includes some defines, including a line like:
// #define of M5STICKC_MAC_ADDRESS_BUGC_ROBOT   {0xNN, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN}
// Replace the N's with the digits of your M5StickC's MAC Address (displayed on screen)
// NEVER check your secrets into a source control manager!
#include "../../Secrets/M5StickCMacAddresses.h"


struct_message  datagram;
struct_response response;
uint8_t         bugAddress[]        = M5STICKC_MAC_ADDRESS_BUGC_ROBOT;    // Specific to the BugC M5StickC
bool            connected           = false;
control_mode    mode                = MODE_FORWARD;


bool command_stop() {
  datagram.speed_0      = 0;
  datagram.speed_1      = 0;
  datagram.speed_2      = 0;
  datagram.speed_3      = 0;
  datagram.color_left   = BLACK;
  datagram.color_right  = BLACK;
  esp_err_t result = esp_now_send(bugAddress, (uint8_t *) &datagram, sizeof(struct_message));
  return (ESP_OK == result);
}


bool command_forward() {
  datagram.speed_0      = 100;
  datagram.speed_1      = -100;
  datagram.speed_2      = 100;
  datagram.speed_3      = -100;
  datagram.color_left   = 0x001000;
  datagram.color_right  = 0x100000;
  esp_err_t result = esp_now_send(bugAddress, (uint8_t *) &datagram, sizeof(struct_message));
  return (ESP_OK == result);
}


bool command_backward() {
  datagram.speed_0      = -100;
  datagram.speed_1      = 100;
  datagram.speed_2      = -100;
  datagram.speed_3      = 100;
  datagram.color_left   = 0x100000;
  datagram.color_right  = 0x001000;
  esp_err_t result = esp_now_send(bugAddress, (uint8_t *) &datagram, sizeof(struct_message));
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


// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status:\t%s\n", (status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
}


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if(!connected) {
    connected = true;
    print_mac_address(TFT_GREEN);
  }
}


void setup() {
  M5.begin();
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
  }
  else if(M5.BtnA.wasPressed()) {
    Serial.println("BtnA pressed");
    M5.Lcd.setTextColor(TFT_GREEN);
    switch(mode) {
      case MODE_FORWARD:
        command_forward();
        M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
        M5.Lcd.drawCentreString("Forward", 80, 64, 2);
        mode = MODE_STOP_AFTER_FORWARD;
        break;
      case MODE_STOP_AFTER_FORWARD:
        M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
        M5.Lcd.drawCentreString("Stop", 80, 64, 2);
        command_stop();
        mode = MODE_BACKWARD;
        break;
      case MODE_BACKWARD:
        command_backward();
        M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
        M5.Lcd.drawCentreString("Backward", 80, 64, 2);
        mode = MODE_STOP_AFTER_BACKWARD;
        break;
      case MODE_STOP_AFTER_BACKWARD:
        command_stop();
        M5.Lcd.fillRect(40, 64, 80, 16, TFT_BLACK);
        M5.Lcd.drawCentreString("Stop", 80, 64, 2);
        mode = MODE_FORWARD;
        break;
    }
  }
  delay(100);
}
