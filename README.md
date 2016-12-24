# WiFi Watt Meter
This is a ESP8266 project for reading "data" from a Watt Meter:  
https://github.com/AlexeySofree/WiFi_Watt_Meter

It's a white color device with 4 buttons in the middle:  
http://www.google.com/images?q=china+wattmeter+white+color+4+buttons

Initially implementation described here:  
http://gizmosnack.blogspot.ru/2014/10/power-plug-energy-meter-hack.html  
http://gizmosnack.blogspot.ru/2014/11/power-plug-energy-meter-now-wireless.html

I implemented the same using ESP8266.

Result data looks like:
~~~~
http://IP
{"name":"WiFiWattMeter","id":8635084,"uptime":"12448.441","obtainedAt":"12448.432","volt":"241.632813","ampere":"0.719168","watt":"97.753906","clientHandleForced":0,"WiFiStatus":1,"freeHeap":38432}

http://IP/?pretty=1
{
  "name": "WiFiWattMeter",
  "id": 8635084,
  "uptime": "12442.948",
  "obtainedAt": "12442.429",
  "volt": "241.765625",
  "ampere": "0.719467",
  "watt": "97.660156",
  "clientHandleForced": 0,
  "WiFiStatus": 1,
  "freeHeap": 38352
}
~~~~

I used Wemos D1 mini and external mini power supply 5V. Built-in power supply can't power ESP8266.
I spent many times debugging code (device reboots after a while) and found a solution:  
avoid using `handleClient` when you have an active interrupt.

I captured data exchange via `Saleae Logic Software` (a `.logicdata` file).  

There are some images of resulted device:  
https://github.com/AlexeySofree/WiFi_Watt_Meter/issues/1
https://cloud.githubusercontent.com/assets/5929124/21467519/5d0836fc-ca0a-11e6-8132-003127f49fee.jpg
https://cloud.githubusercontent.com/assets/5929124/21467520/5e3b2200-ca0a-11e6-82b5-aff88bd34381.jpg

Thanks a lot,  
Alexey Tsarev, Tsarev.Alexey at gmail.com
