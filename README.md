# BugC and BugController - A Pair of Projects for Controlling the M5StickC BugC Robot Hat

The [BugC](https://m5stack.com/collections/m5-hat/products/bugc-w-o-m5stickc) is a simple and inexpensive robot base for the [M5StickC](https://m5stack.com/collections/m5-hat/products/stick-c). Combined with another M5StickC sporting a [Joystick Hat](https://m5stack.com/collections/m5-hat/products/m5stickc-joystick-hat) you have the physical parts of a complete control system.

I had these parts on hand, and was curious about [ESP-Now](https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/), a protocol developed by Espressif Systems to provide direct communications between ESP32 and ESP8266 chips. This seemed like a perfect project for trying out something new.

ESP-Now turns out to be an interesting protocol. Advantages include:

* Connectionless. You don't need a network, just two or more devices.
* Simple. It takes a lot less programming than WiFi or Bluetooth.
* Fast. No discovery, connection, etc.
* It's extremely flexible.
* Surprise bonus: I read claims that you get three times the range. (I have not tested this, but sounds great for remote control.)

Limitations include:

* Non-routable protocol (for local use only, not for remote connections)
* 250 byte maximum payload - fine for light telemetry, not for general communications

## Prerequisites

* PlatformIO rather than the Arduino IDE. You can easily modify these projects to work on Arduino if you don't use PlatformIO; just change main.c to [ProjectName].ino and move everything to one folder per project.
* (2) M5StickCs: [ESP32-PICO Mini IoT Development Kits](https://m5stack.com/collections/m5-hat/products/stick-c)
* (1) BugC: [BugC Programmable Robot Base](https://m5stack.com/collections/m5-hat/products/bugc-w-o-m5stickc)
* (1) Joystick: [Joystick Hat](https://m5stack.com/collections/m5-hat/products/m5stickc-joystick-hat)
* (2) Projects: BugC and BugController
* Approximate cost if you live where I do and order directly from M5Stack in China, about $33.80.

## Setup

* Attach an M5StickC to your BugC when it arrives, turn the BugC switch on and plug the M5StickC in to charge for the night before beginning. It's 750 mAh battery takes a while to charge up, and nothing works until it does.
* Download both projects and locate their directories side-by side, anywhere you like.
* Create another directory in the same location named Secrets.
* Create a file in the Secrets directory named M5StickCMacAddresses.h, and add this to it (you will change this later):

    ```c
    #define M5STICKC_MAC_ADDRESS_BUGC_ROBOT         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    #define M5STICKC_MAC_ADDRESS_BUGC_CONTROLLER    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    ```

* (Why all this trouble?) Never check secrets into a source control manager, much less upload them to github. A MAC address, like a password, is a secret.
* Compile the BugC project and upload to the M5StickC which is attached to the BugC.
* Compile the BugController project and upload to the M5StickC which is attached to the JoyStick.
* **Your Robot Will Not Work** at this point! Please patiently keep reading.
* While web connections use URLs to find other devices, ESP-Now relies on the MAC address, a number which is different for every single device on the Internet. When you turn on your programmed M5StickCs they will display their MAC address in red. It's six paris of hexidecimal digits.
* Return to the Arduino IDE and open Secrets/M5StickCMacAddresses.h
* Enter the address shown by the BugC device into M5STICKC_MAC_ADDRESS_BUGC_ROBOT, and the address on the Joystick device into M5STICKC_MAC_ADDRESS_BUGC_CONTROLLER. For example:
    > 4D 53 12 09 AB FF  
    > ...displayed on the BugC's screen would translate to:

    ```c
    #define M5STICKC_MAC_ADDRESS_BUGC_ROBOT         {0x4D, 0x53, 0x12, 0x09, 0AB, 0xFF}
    ```

* Rebuild both programs and upload them again. Be careful to load the right program to the right M5StickC; you can't swap them because the MAC addresses would be reversed.
* Turn everything off.
* Slide the slide-switch on the back of the BugC base to the right to turn on the BugC.
* Turn on the M5StickC on the BugC. You will see its name in red, its mac address in red, and its battery voltage (green for OK, red for low.)
* Turn on the M5StickC with the Joystick. Both displays should turn from red to green as they connect.

If the two do not connect (text stays red), plug either back into the computer and use the serial monitor to see what errors are being displayed. Hopefully this will help indicate the cause of the problem.  
If the BugC turns green and shows different numbers when you move the controller's joystick but there is no movement, make sure you turned on the BugC itself (it has a slide-switch which should be to the right) and that the base is fully charged.

Once connected, drive your BugC forward by pushing up on the joystick, backwards by pushing down. Left and right are, naturally, left and right.  
If you click the joystick, the red LED in the BugC's M5StickC will turn on as long as the joystick is pressed.  
If your controller M5StickC runs out of power while the BugC is running, pressing the A button (the big one with "M5" on it) on the BugC will stop the motors and turn off the lights.  

It's not a very accurate or powerful robot, but it's fun to play with. My cat does not fear it, but avoids its touch.
