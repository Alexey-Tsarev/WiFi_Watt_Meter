/*
 * This is a ESP8266 project for reading data from a Watt Meter:
 * https://github.com/AlexeySofree/WiFi_Watt_Meter
 * Alexey Tsarev, Tsarev.Alexey at gmail.com
*/

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "Elapser.cpp"
#include "Uptimer.cpp"

// Main config
#define configPinCLK 5
#define configPinSDO 4

#define configWiFiManagerIsActive
#define configWiFiManagerAPName               "WiFiWattMeter"
#define configWiFiManagerConfigTimeoutSeconds 120

#define configHTTPPort 80
#define configSerialSpeed 115200
//#define configOTAisActive // Only for a development
// End Main config

// Blynk сonfig
//#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG
//#define configBlinkSSL
#ifdef configBlinkSSL
#include <BlynkSimpleEsp8266_SSL.h>
#else

#include <BlynkSimpleEsp8266.h>

#endif
#define blynkAuthToken         "" // Blynk auth token
#define blynkHost              ""
#define blynkPort              0
#define blynkSSLFingerprint    ""
#define blynkSendDataInSeconds 0
uint8_t vPinVolt = 0;
uint8_t vPinAmpere = 1;
uint8_t vPinWatt = 2;
// End Blynk сonfig

#define dataMaxBytes 24
volatile bool CLKSynced, nextBit, pinSDOStatus;
volatile uint32_t lastCLKRisedMicros, lastCLKSyncLenMicros, CLKSyncLenMicros;
bool WiFiConnectedFlag, interruptActiveFlag, clientHandledFlag, clientHandleForcedFlag, blynkFlag, blynkSSLFlag;
uint8_t data[dataMaxBytes], dataOffset, curByte, curBitOffset;
char strBuf[512], strBuf2[512];
uint32_t i, j, CLKSyncMinLenMicros = 1000, CLKSyncMaxLenMicros = 5000, disableInterruptWithinMicros = 580000, nextBitTimeoutMicros = 300000, clientMaxWaitMicros = 2500000, lastDoSyncStartMicros, lastDoSyncFinishMicros, lastClientHandledMicros, lastBlynkSentDataSec;
double volt, ampere, watt;
char voltStr[16] = "V (n/a)", ampereStr[16] = "A (n/a)", wattStr[16] = "W (n/a)", obtainedAtStr[16];

uint32_t Elapser::lastTime;
Uptimer uptimer;
ESP8266WebServer server(configHTTPPort);


// Functions
void lg(const char s[]) {
    Serial.print(s);
}


void log(const char s[] = "") {
    Serial.println(s);
}


void BlynkConnectionConfigure(bool connect = false) {
    blynkFlag = strlen(blynkAuthToken) > 0;

    if (blynkFlag) {
        lg("Blynk: configure connection... ");

        blynkSSLFlag = strlen(blynkSSLFingerprint) > 0;
#ifdef configBlinkSSL
        if (blynkSSLFlag)
                Blynk.config(blynkAuthToken, blynkHost, blynkPort, blynkSSLFingerprint);
#endif
        if (!blynkSSLFlag)
            if (strlen(blynkHost))
                if (blynkPort)
                    Blynk.config(blynkAuthToken, blynkHost, blynkPort);
                else
                    Blynk.config(blynkAuthToken, blynkHost);
            else
                Blynk.config(blynkAuthToken);

        log("complete");

        if (connect) {
            log("Blynk: connecting");
            Blynk.connect();
        }
    }
}


BLYNK_CONNECTED() {
    log("Blynk: connected");
}


BLYNK_READ_DEFAULT() {
    uint8_t pin = request.pin;
    snprintf(strBuf, sizeof(strBuf), "Blynk: read pin: %u", pin);
    log(strBuf);

    if (pin == vPinVolt)
        Blynk.virtualWrite(pin, voltStr);
    else if (pin == vPinAmpere)
        Blynk.virtualWrite(pin, ampereStr);
    else if (pin == vPinWatt)
        Blynk.virtualWrite(pin, wattStr);
}


void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            if (!WiFiConnectedFlag) {
                WiFiConnectedFlag = true;

                lg("WiFi: connected to ");
                lg(WiFi.SSID().c_str());
                lg(" / ");
                log(WiFi.localIP().toString().c_str());

                BlynkConnectionConfigure();
            }

            break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
            if (WiFiConnectedFlag) {
                WiFiConnectedFlag = false;

                log("WiFi: disconnected");
            }

            break;
    }
}


void APConfigCallback(WiFiManager *myWiFiManager) {
}


void getPrintableData(char out[]) {
    sprintf(out, "[%02u]: ", dataOffset);
    uint8_t len = dataOffset - 1;

    for (uint8_t i = 0; i < dataOffset; i++) {
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

    uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
    json["uptime"] = strBuf2;

    json["obtainedAt"] = obtainedAtStr;
    json["volt"] = voltStr;
    json["ampere"] = ampereStr;
    json["watt"] = wattStr;
    json["clientHandleForced"] = uint8_t(clientHandleForcedFlag);
    json["WiFiStatus"] = uint8_t(WiFiConnectedFlag);
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
            lastCLKSyncLenMicros = Elapser::getElapsedTime(lastCLKRisedMicros, microSec);

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
        if (Elapser::isElapsedTimeFromStart((uint32_t &) lastCLKRisedMicros, nextBitTimeoutMicros, microSec)) {
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

    //if ((data[5] == 144) || (data[5] == 146) || (data[5] == 208) || (data[5] == 210))
    if ((data[5] & 0b10010000) == 0b10010000)
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

                i = Elapser::getElapsedTime(lastDoSyncStartMicros, microSec);
                lastDoSyncFinishMicros = Elapser::getLastTime();

                uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
                sprintf(strBuf, "%s doSyncMicros: %lu, syncLenMicros: %lu, ", strBuf2, i, CLKSyncLenMicros);
                getPrintableData(strBuf + strlen(strBuf));

                if (data[21] != 255) {
                    uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));

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

                if (blynkFlag && WiFiConnectedFlag &&
                    ((blynkSendDataInSeconds == 0) ||
                     Elapser::isElapsedTimeFromStart(lastBlynkSentDataSec, (uint32_t) blynkSendDataInSeconds, sec,
                                                     true))) {
                    uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
                    sprintf(strBuf, "%s Blynk: send data", strBuf2);
                    log(strBuf);

                    Blynk.virtualWrite(vPinVolt, voltStr);
                    Blynk.virtualWrite(vPinAmpere, ampereStr);
                    Blynk.virtualWrite(vPinWatt, wattStr);
                }
            }
        }
    } else if (Elapser::isElapsedTimeFromStart(lastDoSyncFinishMicros, disableInterruptWithinMicros, microSec))
        attachInt();

    if (interruptActiveFlag &&
        Elapser::isElapsedTimeFromStart(lastClientHandledMicros, clientMaxWaitMicros, microSec)) {
        detachInt();
        clientHandleForcedFlag = true;

        uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
        sprintf(strBuf, "%s clientHandleForced", strBuf2);
        log(strBuf);
    }

    // Handle when no interrupt
    if (!interruptActiveFlag) {
#ifdef configOTAisActive
        ArduinoOTA.handle();
#endif
        i = 0;

        server.handleClient();

        while (clientHandledFlag) {
            clientHandledFlag = false;
            i++;
            yield();
            server.handleClient();
        }

        lastClientHandledMicros = micros();

        if (i) {
            uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
            sprintf(strBuf, "%s Handled client(s): %lu", strBuf2, i);
            log(strBuf);
        }

        if (blynkFlag && WiFiConnectedFlag)
            Blynk.run();

        if (clientHandleForcedFlag) {
            clientHandleForcedFlag = false;
            attachInt();
        }
    }
    // End Handle when no interrupt
}
