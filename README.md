# OpenNETek

You have a Smart Inverter with a name like SG600MD and it looks like this? 

![sample image of newenergytek microinverter](https://github.com/MichaelHeimann/OpenNETek/blob/master/newenergytek%20inverter.jpg?raw=true)

Then this might be of use to you :)

This repo is inspired by a comment in the NETSGPClient Project ( https://github.com/atc1441/NETSGPClient/issues/13#issuecomment-1147458860 ) which was a quick and dirty way of throwing some example code together and the result was a firmware for an esp that offers a webgui and mqtt capabilities. I loved it so much that I created this here to iron out some issues.

This project offers esp32 firmware that improves inverters from newenergytek ( www.newenergytek.com ) sold under many brands. They can be bought from AliExpress, Amazon etc. .
They have very limited monitoring capabilities via proprietary "databoxes" which transfer the serial interface unencrypted over the air.

The idea is to put an ESP32 into the inverter to connect it to wifi. The ESP reports performance data via MQTT and offers a simple web page - all without the need of any cloud service.

Guide:

Hardware modification:
* open inverter
![open inverter with LC12S](https://github.com/MichaelHeimann/OpenNETek/blob/master/inverter%20with%20LC12S.jpg?raw=true)
* remove LC12S wireless uart module "LC12S" (6 pins, 4 used)
![inverter without LC12S](https://github.com/MichaelHeimann/OpenNETek/blob/master/inverter%20without%20LC12S.jpg?raw=true)
* flash image onto your ESP32 (depending on your ESP32 connection to the inverter, you might power the inverter logic when connecting USB to the ESP32 to flash it. So better do it unconnected) You can update the firmware later using http://<ip_of_ESP>/update without usb connection. This way the esp can stay in the inverter and still receive firmware updates.
* connect ESP32 to the now free 3.3v, GND, RX and TX pins where the LC12S was connected to. Sorry for black not being ground, the connector came this way and I was to lazy to change colors. pinout is:
  - white is ground
  - yellow is RX (which goes to TX of the ESP32, which is PIN17)
  - black is TX (which goes to RX of the ESP32, which is PIN16)
  - red is 3.3v
![inverter with cables to ESP32](https://github.com/MichaelHeimann/OpenNETek/blob/master/inverter%20cables%20to%20ESP32.jpg?raw=true)
* (optional) remove blue plastic on thermal pads if neccessary. mine were still on. (also on the back of the mainboard)
![inverter with plastic on thermal pads](https://github.com/MichaelHeimann/OpenNETek/blob/master/inverter%20blue%20plastic%20wtf.jpg?raw=true)

Configuration:
* after connecting DC power to the inverter, the ESP32 initializes
* after a minute or so: connect to SSID:OpenNETek Pass:opennetek!
* a captive portal pops up and you can enter wifi, mqtt details and the inverterid from the chassis
* after that, the inverter publishes performance data via mqtt and http

I suggest using an ESP32 with the possibility to connect an external antenna. This way you can repurpose the hole (and even the antenna) from the LC12S.
I used an ESP32 WROOM 32U.

Beware: some ESP32 with external antenna socket and internal antenna need hardware modification to "switch" to the external antenna.

<a href="https://www.buymeacoffee.com/Highman" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>
