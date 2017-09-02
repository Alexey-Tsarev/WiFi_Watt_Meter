#include <Arduino.h>


class Uptimer {
private:
    uint32_t curMillis, prevMillis, addSec;
    uint16_t addMillis;

public:
    void update() {
        curMillis = millis();

        if (prevMillis > curMillis) {
            addSec += 4294967;
            addMillis += 296;

            if (addMillis >= 1000) {
                addMillis -= 1000;
                addSec++;
            }
        }

        prevMillis = curMillis;
    }


    void returnUptime(uint32_t &seconds, uint16_t &milliSeconds) {
        update();

        seconds = curMillis / 1000;
        milliSeconds = curMillis - seconds * 1000;

        if (addMillis != 0) {
            milliSeconds += addMillis;

            if (milliSeconds >= 1000) {
                milliSeconds -= 1000;
                seconds++;
            }
        }

        if (addSec != 0)
            seconds += addSec;
    }


    void returnUptimeStr(char *str, uint16_t strLen) {
        uint32_t seconds;
        uint16_t milliSeconds;

        returnUptime(seconds, milliSeconds);

        snprintf(str, strLen, "%lu.%03i", seconds, milliSeconds);
    }
};
