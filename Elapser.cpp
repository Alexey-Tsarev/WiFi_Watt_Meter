#include <Arduino.h>


enum TimePrecision {
    microSec,
    milliSec,
    sec
};


// default time precision is micro seconds
class Elapser {
private:
    static uint32_t lastTime;

public:
    static uint32_t getLastTime() {
        return lastTime;
    }

    static uint32_t getElapsedTime(uint32_t start, uint32_t &curTime, TimePrecision timePrecision = microSec) {
        double curTimeSec;

        if (timePrecision == microSec)
            curTime = micros();
        else {
            curTime = millis();

            if (timePrecision == sec) {
                curTimeSec = curTime / 1000;
                curTime = curTimeSec;
            }
        }

        lastTime = curTime;

        if (curTime >= start)
            return curTime - start;
        else if (timePrecision == sec)
            return 4294967.296 - start + curTimeSec;
        else
            return 0xFFFFFFFF - start + 1 + curTime;
    }


    static uint32_t getElapsedTime(uint32_t start, TimePrecision timePrecision = microSec) {
        uint32_t curTime;
        return getElapsedTime(start, curTime, timePrecision);
    }


    static bool isElapsedTimeFromStart(uint32_t &start, uint32_t elapsed, TimePrecision timePrecision = microSec, bool updateStart = false) {
        bool elapse = getElapsedTime(start, timePrecision) >= elapsed;

        if (elapse && updateStart)
            start = lastTime;

        return elapse;
    }
};
