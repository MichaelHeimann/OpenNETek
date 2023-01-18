# OpenNETek

Based on a comment in the NETSGPClient Project ( https://github.com/atc1441/NETSGPClient/issues/13#issuecomment-1147458860 ) which uses 
WifiManager, WebSerial, PubSubClient. Thanks a lot!

This project wants to improve inverters from newenergytek (www.newenergytek.com ) sold as whitelabel inverters. They can be bought from several brands via AliExpress, Amazon etc. .
(At least some) have very limited monitoring capabilities or need proprietary "databoxes" which transfer the serial interface unencrypted over the air.

The idea is to put an ESP/arduino into the inverter, use wifi to report performance data via MQTT and offer a simple web page without the need of any cloud service.

Guide:

Hardware:
* flash image on your ESP32
* open inverter
* remove LC12S wireless uart module "LC12S" (5 pins, 4 used)
* connect ESP32 to the now free 3.3v, GND, RX and TX pins where the LC12S was connected to

Configuration:
* after connecting DC power to the inverter, the ESP32 initializes
* after a minute or so: connect to SSID:OpenNETek Pass:opennetek!
* a captive portal pops up and you can enter wifi, mqtt details and the inverterid from the chassis
* after that, the inverter publishes performance data via mqtt and http

I suggest using an ESP32 with the possibility to connect an external antenna. This way you can repurpose the hole (and even the antenna) from the LC12S.
I used an ESP32 WROOM 32U.

Beware: some ESP32 with external antenna socket and internal antenna need hardware modification to "switch" to the external antenna.

<a href="https://www.buymeacoffee.com/Highman" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>
