
/*\
|*| ============================================================
|*| LaserClock_v2.ino
|*| 
|*| POV Laser Clock
|*| 
|*| written May 15, 2022 trent m. wyatt
|*| v1.0.0  Initial writing. Tests.
|*|         May 20, 2022 trent m. wyatt
|*| 
|*| v1.0.1  June 3, 2022 ++tmw
|*|         First working version. No timers!
|*| 
|*| ============================================================
\*/

#include <Arduino.h>
#include <digitalWriteFast.h>
#include <inttypes.h>

//#include <Printf.h>

enum MagicNumbers : uint32_t { 
    // Pin Usage: adjust as needed..
    loLaserPin  =    4,
    hiLaserPin  =    5,
    motorPin    =    6,
    pulsePin    =    2,
    ledPin      =   13,

    // offset of foreground to ISR - adjust so that 12:00 is at the top
    offset      =   41,

    // time stuff
    secsPerMin  =   60,
    minsPerHour =   60,
    secsPerHour = 3600,
};

volatile   uint16_t    pulses;                  // updated once a second by the ISR
volatile   uint16_t    pulses_working;          // used by ISR for IR detector interrupt count
volatile   bool        new_pulses;              // indicates 'pulses' has updated value when toggled
volatile   bool        ticks;                   // indicates a pulse when toggled
volatile   uint16_t    hour, minute, second;    // the current time    
uint32_t   startTime;                           // the time we started - last compile time adjusted for upload time.

#define    MOTORON         digitalWriteFast(motorPin, HIGH)
#define    MOTOROFF        digitalWriteFast(motorPin, LOW)

#define    LASERONHI       digitalWriteFast(hiLaserPin, HIGH)
#define    LASEROFFLO      digitalWriteFast(loLaserPin, LOW)
#define    LASERONLO       digitalWriteFast(loLaserPin, HIGH)
#define    LASEROFFHI      digitalWriteFast(hiLaserPin, LOW)
#define    LASEROFF        {LASEROFFHI; LASEROFFLO;}

#define    LEDON           digitalWriteFast(ledPin, HIGH)
#define    LEDOFF          digitalWriteFast(ledPin, LOW)
#define    LEDTOGGLE       if (digitalReadFast(ledPin)) { digitalWriteFast(ledPin, LOW); } else { digitalWriteFast(ledPin, HIGH); }


void updateTime() {
    uint32_t now_secs = micros() / 1000000UL + startTime;
    hour = now_secs / secsPerHour;
    now_secs -= hour * secsPerHour;

    minute = now_secs / secsPerMin;
    now_secs -= minute * secsPerMin;

    second = now_secs;
}


// 
// Interrupt Service Routine (ISR) for External Interrupt 0 (pin 2 on Uno, Nano)
// 
void pulse() {
    uint32_t now = micros();
    static uint32_t timer = now;
    ticks = !ticks;

    ++pulses_working;

    if (now - timer >= 1000000UL) {
        timer = now;
        pulses = pulses_working;
        pulses_working = 0;
        new_pulses = !new_pulses;
        LEDTOGGLE;
    }
}


void setup() {
    // 
    // Set the time based on the compile time:
    // 
    char const tm[9] = __TIME__;
    hour   = ((uint32_t)(tm[0] - '0') * 10UL) + (uint32_t)(tm[1] - '0');
    minute = ((uint32_t)(tm[3] - '0') * 10UL) + (uint32_t)(tm[4] - '0');
    second = ((uint32_t)(tm[6] - '0') * 10UL) + (uint32_t)(tm[7] - '0');

    // 
    // Adjust for the time it took to upload: (change time as needed)
    // 
    uint32_t upload_time_seconds = 6UL;

    second += upload_time_seconds;

    while (second >= secsPerMin) {
        second -= secsPerMin;
        if (++minute >= minsPerHour) {
            minute -= minsPerHour;
            if (++hour >= 24UL) {
                hour -= 24UL;
            }
        }
    }

    // 
    // Set the starting time in seconds since midnight:
    // 
    startTime = hour * secsPerHour + minute * secsPerMin + second;

    // 
    // Set up the pin modes:
    // 
    pinModeFast(loLaserPin, OUTPUT);
    pinModeFast(hiLaserPin, OUTPUT);
    pinModeFast(motorPin,   OUTPUT);
    pinModeFast(pulsePin,   INPUT);
    pinModeFast(ledPin,     OUTPUT);

    LEDOFF;
    LASEROFF;
    MOTOROFF;

    // Self-Tests: Operate each device for 2 seconds:
    LASERONLO; 
    delay(2000); 
    LASEROFFLO;
    delay(2000); 
    
    LASERONHI; 
    delay(2000); 
    LASEROFFHI;
    delay(2000); 
    
    // 
    // Start the pulse detector interrupts
    // 
    attachInterrupt(digitalPinToInterrupt(pulsePin), pulse, FALLING);

    MOTORON;
    delay(2000);
    
    // for debug
    Serial.begin(115200);

    if (0 == pulses) {
        Serial.print("\n\nError: No interrupts from IR pulse emitter/detector\n");
        MOTOROFF;
        Serial.print("halted.\n");
        for (;;) {
            LEDON;
            LASEROFFHI;
            delay(200);
            LASERONHI;
            LEDOFF;
            delay(200);
        }
    }
}


void loop() 
{
    static volatile bool lastTicks = ticks;

    while (lastTicks == ticks);

    lastTicks = !lastTicks;
    updateTime();

    uint16_t us_per_rev = 1000000UL / pulses;
    uint16_t us_per_sec = us_per_rev / secsPerMin;

    enum {
        dim=1, 
        medium=2, 
        high=3

    } bright;


    for (int16_t k=59; k >= 0; k--) {
        int16_t secIndex = (k - offset); if (secIndex < 0) secIndex += secsPerMin;
        int16_t onTime = 10;
        bright = dim;

        uint16_t hand = (secIndex / 5);
        if (hand == (hour % 12) && (secIndex % 5) < 2) {
            onTime = us_per_sec / 2;
            bright = medium;
        }
        else
        if (secIndex == int(minute)) {
            onTime = us_per_sec / 2;
            bright = medium;
        }
        else
        if (secIndex == int(second)) {
            //onTime = 35;
            onTime = us_per_sec / 2;
            bright = medium;
        }

        if ((secIndex % 5) == 0) {
            bright = high;
        }

        switch (bright) {
            case dim:
                LASERONLO;
                break;
            case medium:
                LASERONHI;
                break;
            case high:
                LASERONLO;
                LASERONHI;
                break;
        }

        delayMicroseconds(onTime);
        LASEROFF;

        if (k != 0) {
            delayMicroseconds((us_per_sec - onTime) - (pulses - 15));
        }
    }

//    printf("%02lu:%02lu:%02lu pulses: %u\n", hour, minute, second, pulses);
}
