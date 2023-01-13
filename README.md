# OpenDTU-qnd

Based on a comment in the NETSGPClient Project ( https://github.com/atc1441/NETSGPClient/issues/13#issuecomment-1147458860 ) Thanks a lot!

This project wants to improve inverters from newenergytek (www.newenergytek.com ) sold under different names via AliExpress, Amazon etc. (at least some) with limited smarthome or monitoring capabilities.

The idea is to put an ESP/arduino into the inverter to enable wifi and report performance data via MQTT and via a simple web page without the need of a cloud service.

So, the guide is:

- open inverter
- remove LC12S wireless uart (unencrypted)
- flash image on your ESP32
- connect ESP32 to the now free 3.3v, GND, RX and TX pins of the former LC12S
- after connecting DC power to the inverter, the ESP32 initializes
- after a minute or so: connect to SSID:OpenDTU Pass:opendtu!
- a captive portal pops up and you can enter wifi, mqtt details and the inverterid from the chassis
- after that, the inverter publishes performance data via mqtt and http

I suggest using a ESP32 with possibility to connect an external antenna to repurpose the one from the LC12S.
I used a ESP32 WROOM 32U.