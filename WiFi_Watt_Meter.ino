/*
 * This is a ESP8266 project for reading data from a Watt Meter:
 * https://github.com/AlexeySofree/WiFi_Watt_Meter
*/

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

// Cfg
#define configPinCLK 5
#define configPinSDO 4

#define configWiFiManagerIsActive
#define configWiFiManagerAPName               "WiFiWattMeter"
#define configWiFiManagerConfigTimeoutSeconds 120

#define configHTTPPort 80
#define configSerialSpeed 115200
//#define configOTAisActive // Only for a development
//#define configWDTO WDTO_8S
// End Cfg

#define dataMaxBytes 24
volatile bool CLKSynced, nextBit, pinSDOStatus;
volatile unsigned long lastCLKRisedMicros, lastCLKSyncLenMicros, CLKSyncLenMicros;
bool WiFiConnectedFlag, interruptActiveFlag, clientHandledFlag, clientHandleForcedFlag;
byte data[dataMaxBytes], dataOffset, curByte, curBitOffset;
char strBuf[512], strBuf2[512];
unsigned long i, j, curMicros, CLKSyncMinLenMicros = 1000, CLKSyncMaxLenMicros = 5000, disableInterruptWithinMicros = 580000, nextBitTimeoutMicros = 300000, clientMaxWaitMicros = 2500000, lastDoSyncStartMicros, lastDoSyncFinishMicros, lastClientHandledMicros, uptimeAddMillis, uptimeAddSec, prevMillis;
double volt, ampere, watt;
char voltStr[16], ampereStr[16], wattStr[16], obtainedAtStr[16];

ESP8266WebServer server(configHTTPPort);


// Functions
void lg(const char s[]) {
    Serial.print(s);
}


void log(const char s[] = "") {
    Serial.println(s);
}


unsigned long getElapsedMicros(unsigned long start) {
    curMicros = micros();

    if (curMicros >= start)
        return curMicros - start;
    else
        return 0xFFFFFFFF - start + curMicros + 1;
}


bool isElapsedMicrosFromStart(unsigned long start, unsigned long elapsed) {
    return getElapsedMicros(start) >= elapsed;
}


void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            if (!WiFiConnectedFlag) {
                WiFiConnectedFlag = true;

                log("WiFi is ON");
                lg("Connected to: ");
                lg(WiFi.SSID().c_str());
                lg(" / ");
                log(WiFi.localIP().toString().c_str());
            }

            break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
            if (WiFiConnectedFlag) {
                WiFiConnectedFlag = false;

                log("WiFi is OFF");
            }

            break;
    }
}


void APConfigCallback(WiFiManager *myWiFiManager) {
}


void getCurTS(char out[]) {
    unsigned long curMillis = millis();
    unsigned long sec = curMillis / 1000;
    unsigned int millis = curMillis - sec * 1000;

    if (prevMillis > curMillis) {
        uptimeAddSec += 4294967;
        uptimeAddMillis += 296;
    }

    millis += uptimeAddMillis;

    if (millis >= 1000) {
        sec++;
        millis -= 1000;
    }

    sec += uptimeAddSec;
    prevMillis = curMillis;

    sprintf(out, "%lu.%03i", sec, millis);
}


void getPrintableData(char out[]) {
    sprintf(out, "[%02u]: ", dataOffset);
    byte len = dataOffset - 1;

    for (byte i = 0; i < dataOffset; i++) {
        sprintf(out + strlen(out), "%03u", data[i]);

        if (i != len)
            if (((i + 1) % 8) == 0)
                strcat(out, " | ");
            else
                strcat(out, " ");
    }
}


void sendResponse(const char data[]) {
    server.send(200, "text/plain", data);
    clientHandledFlag = true;
}


void handleURIRoot() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    json["name"] = configWiFiManagerAPName;
    json["id"] = ESP.getChipId();

    getCurTS(strBuf2);
    json["uptime"] = strBuf2;

    json["obtainedAt"] = obtainedAtStr;
    json["volt"] = voltStr;
    json["ampere"] = ampereStr;
    json["watt"] = wattStr;
    json["clientHandleForced"] = byte(clientHandleForcedFlag);
    json["WiFiStatus"] = byte(WiFiConnectedFlag);
    json["freeHeap"] = ESP.getFreeHeap();

    if (server.arg("pretty") == "1")
        json.prettyPrintTo(strBuf, sizeof(strBuf));
    else
        json.printTo(strBuf, sizeof(strBuf));

    sendResponse(strBuf);
}


void handleURIData() {
    getPrintableData(strBuf);
    sendResponse(strBuf);
}


void handleURITest() {
    sendResponse("test");
}


void handleURIReset() {
    sendResponse("reset");
    ESP.reset();
}


void handleURIRestart() {
    sendResponse("restart");
    ESP.restart();
}


void startHTTP() {
    lg("Starting HTTP Web Server... ");

    server.on("/", handleURIRoot);
    server.on("/data", handleURIData);
    server.on("/reset", handleURIReset);
    server.on("/restart", handleURIRestart);
    server.on("/test", handleURITest);

    server.begin();

    log("done");
}


void interruptOnCLKChanged() {
    if (digitalRead(configPinCLK) == HIGH) {
        // Rising
        lastCLKRisedMicros = micros();

        if (CLKSynced) {
            if (nextBit) {
                CLKSynced = false;
                return;
            }

            pinSDOStatus = digitalRead(configPinSDO) == HIGH;
            nextBit = true;
        }
        // End Rising
    } else {
        // Falling
        if (!CLKSynced) {
            lastCLKSyncLenMicros = getElapsedMicros(lastCLKRisedMicros);

            if ((lastCLKSyncLenMicros > CLKSyncMinLenMicros) && (lastCLKSyncLenMicros < CLKSyncMaxLenMicros)) {
                CLKSyncLenMicros = lastCLKSyncLenMicros;
                CLKSynced = true;
                nextBit = false;
            }
        }
        // End Falling
    }
}


bool readBit() {
    while (!nextBit) {
        if (isElapsedMicrosFromStart(lastCLKRisedMicros, nextBitTimeoutMicros)) {
            CLKSynced = false;
            return false;
        } else {
//            yield(); // If this active, then better stability, but read failed sometimes
        }
    }

    if (pinSDOStatus)
        curByte++;

    if (curBitOffset == 7) {
        data[dataOffset] = curByte;
        dataOffset++;
        curByte = 0;
        curBitOffset = 0;
    } else {
        curByte <<= 1;
        curBitOffset++;
    }

    nextBit = false;
    return true;
}


void doSync() {
    dataOffset = 0;
    curByte = 0;
    curBitOffset = 0;

    do {
        if (!readBit())
            return;
    } while (dataOffset < 6);

    if ((data[5] == 144) || (data[5] == 208))
        do {
            if (!readBit())
                return;
        } while (dataOffset < dataMaxBytes);

    CLKSynced = false;
}
// End Functions


void setup() {
    Serial.begin(configSerialSpeed);

    while (!Serial)
        yield();

    log("Start");

    pinMode(configPinCLK, INPUT);
    pinMode(configPinSDO, INPUT);

    WiFi.onEvent(WiFiEvent);

#ifdef configWDTO
    ESP.wdtEnable(configWDTO);
#endif

#ifdef configWiFiManagerIsActive
    log("Connect or setup WiFi");

    if (WiFi.status() != WL_CONNECTED) {
        WiFiManager wifiManager;

        // Reset settings, only for testing! This will reset your current WiFi settings
        //wifiManager.resetSettings();

        wifiManager.setTimeout(configWiFiManagerConfigTimeoutSeconds);
        wifiManager.setAPCallback(APConfigCallback);

        sprintf(strBuf, "%s-%i", configWiFiManagerAPName, micros());
        sprintf(strBuf2, "%08i", ESP.getChipId());

        if (!wifiManager.autoConnect(strBuf, strBuf2))
            log("Timeout");
    }
#endif

#ifdef configOTAisActive
    ArduinoOTA.begin();
#endif

    startHTTP();
}


void attachInt() {
    attachInterrupt(digitalPinToInterrupt(configPinCLK), interruptOnCLKChanged, CHANGE);
    interruptActiveFlag = true;
}


void detachInt() {
    detachInterrupt(digitalPinToInterrupt(configPinCLK));
    interruptActiveFlag = false;
}


void loop() {
    if (interruptActiveFlag) {
        if (CLKSynced) {
            lastDoSyncStartMicros = micros();
            doSync();

            if (dataOffset == dataMaxBytes) {
                detachInt();

                i = getElapsedMicros(lastDoSyncStartMicros);
                lastDoSyncFinishMicros = curMicros;

                getCurTS(strBuf2);
                sprintf(strBuf, "%s doSyncMicros: %lu, syncLenMicros: %lu, ", strBuf2, i, CLKSyncLenMicros);
                getPrintableData(strBuf + strlen(strBuf));

                if (data[21] != 255) {
                    getCurTS(obtainedAtStr);

                    volt = (data[13] + double(data[14]) / 256) * 2;
                    dtostrf(volt, 0, 6, voltStr);

                    // the fact is that the real formula is unknown
                    //ampere = (data[17] * 256 + (data[18] - 3) + double(data[19] - 12) / 256) / 2000;
                    ampere = (data[17] * 256 + data[18] + double(data[19]) / 256) / 2000;
                    dtostrf(ampere, 0, 6, ampereStr);

                    watt = (data[21] * 256 + data[22] + double(data[23]) / 256) / 2;
                    dtostrf(watt, 0, 6, wattStr);

                    sprintf(strBuf + strlen(strBuf), "\n%s %s %s %s", obtainedAtStr, voltStr, ampereStr, wattStr);
                }

                log(strBuf);
            }
        }
    } else if (isElapsedMicrosFromStart(lastDoSyncFinishMicros, disableInterruptWithinMicros))
        attachInt();

    if (interruptActiveFlag && isElapsedMicrosFromStart(lastClientHandledMicros, clientMaxWaitMicros)) {
        clientHandleForcedFlag = true;
        detachInt();
        lg("clientHandleForced ");
    }

    if (!interruptActiveFlag || clientHandleForcedFlag) {
#ifdef configOTAisActive
        ArduinoOTA.handle();
#endif
        i = 0;

        do {
            yield();
            clientHandledFlag = false;
            server.handleClient(); // handle when no interrupt!
            i++;
        } while (clientHandledFlag);

        if (i >= 2) {
            getCurTS(strBuf2);
            sprintf(strBuf, "%s Handled client(s): %lu", strBuf2, i - 1);
            log(strBuf);
        }

        lastClientHandledMicros = micros();
    }

    if (clientHandleForcedFlag) {
        clientHandleForcedFlag = false;
        attachInt();

        if (i == 1)
            log();
    }
}
