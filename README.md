# WiFi Watt Meter
This is a ESP8266 project for reading "data" from a Watt Meter:  
https://github.com/AlexeySofree/WiFi_Watt_Meter

It's a white color device with 4 buttons in the middle:  
http://www.google.com/images?q=china+wattmeter+white+color+4+buttons

Initially implementation described here:  
http://gizmosnack.blogspot.ru/2014/10/power-plug-energy-meter-hack.html  
http://gizmosnack.blogspot.ru/2014/11/power-plug-energy-meter-now-wireless.html

I implemented the same using ESP8266.

Result data look like:
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

I used Wemos D1 mini and external 5V mini power supply. Built-in power supply can't power ESP8266.
I spent many times debugging code (device reboots after a while) and found a solution:  
avoid using `handleClient` when you have an active interrupt.

An example of raw data, captured via `Saleae Logic Software` provided in a `.logicdata` file.

You can easily collect data via Zabbix. A collector file provided:  
`php WiFi_Watt_Meter_2_Zabbix.php`
~~~~
Run: echo "wifi-watt-meter volt 241.726563
wifi-watt-meter ampere 0.187865
wifi-watt-meter watt 28.933594
wifi-watt-meter freeHeap 38432
wifi-watt-meter uptime 716.726
wifi-watt-meter obtainedAt 716.595
wifi-watt-meter clientHandleForced 0" | zabbix_sender -z 127.0.0.1 -i -
Output: Array
(
    [0] => info from server: "processed: 7; failed: 0; total: 7; seconds spent: 0.000145"
    [1] => sent: 7; skipped: 0; total: 7
)
~~~~

There are some images of the resulted device:  
https://github.com/AlexeySofree/WiFi_Watt_Meter/issues/1

Thanks a lot,  
Alexey Tsarev, Tsarev.Alexey at gmail.com
