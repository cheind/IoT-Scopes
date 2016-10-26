
#include "Arduino.h"

// Include library
#include <DigitalScope.h>

#define NEVENTS 128
cheind::DigitalScope<NEVENTS> scope(2);

bool firstEvent = false;

void setup()
{
    Serial.begin(9600);
    while(!Serial) {}

    // Set our callback function when data gathering is complete. 
    scope.setFirstCallback(onFirst);

    // Start recording.
    firstEvent = false;
    scope.start();
}

void loop()
{
    if (firstEvent) {
        // Sleep for a second
        delay(1000);
        
        scope.stop();
        for (uint16_t i = 0; i < scope.numEvents(); ++i)
        {
            Serial.print(scope.timeOf(i)); Serial.print(",");            
            Serial.print(scope.eventOf(i));

            if (i < NEVENTS - 1)
                Serial.print(" ");
            Serial.println();
        }

        // Restart the scope after a short pause.
        delay(5000);
        Serial.println("Ready for capture");
        firstEvent = false;
        scope.start();
    }
}

/**
    Callback function from DigitalScope enough events have been gathered.

    This function is handed to scope through DigitalScope::setFirstCallback in 
    setup(), before the scope is started. 

    Note, this function is invoked from an interrupt service routine (ISR), therefore
    delay and delayMicroseconds will not work in here. As a rule of thumb, just toggle
    some flags in here that signal loop() what todo.  
    
    Read more about interrupts.
    https://www.arduino.cc/en/Reference/AttachInterrupt
*/
void onFirst() 
{
    // Signal loop() that first event was recorded.
    firstEvent = true;
}