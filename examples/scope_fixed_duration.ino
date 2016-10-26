
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

    scope.start(RISING);
}

void loop()
{
    if (firstEvent) {
        // Sleep for a second. Scope will continue to record data meanwhile.
        delay(1000);
        
        // Explicitly stop scope
        scope.stop();

        const uint16_t nEvents = scope.numEvents();
        Serial.println("BEGIN DATA");
        for (uint16_t i = 0; i < nEvents; ++i)
        {
            // Print info about event time in microseconds 
            // since first event, the current state HIGH/LOW, the event type triggering
            // the state change RISING/FALLING.
            Serial.print(scope.timeOf(i)); Serial.print(" ");            
            Serial.print(scope.eventOf(i)); Serial.print(" ");
            Serial.print(scope.stateOf(i));
            Serial.println();
        }
        Serial.println("END DATA");

        // Restart the scope after a short pause.
        delay(5000);
        Serial.println("LOG Ready for capture");
        firstEvent = false;
        scope.start(RISING);
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