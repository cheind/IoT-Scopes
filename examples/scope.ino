
#include "Arduino.h"

// Include library
#include <DigitalScope.h>

// Number of events to collect
#define NEVENTS 128

// Initialize a scope from the number of events and the target pin(2)
cheind::DigitalScope<NEVENTS> scope(2);

// A flag used to signal the main loop that NEVENTS samples have been captured.
bool complete = false;

void setup()
{
    Serial.begin(9600);
    while(!Serial) {}

    // Set our callback function when data gathering is complete. 
    scope.setCompletedCallback(onComplete);

    // Start recording.
    complete = false;
    scope.start();
}

void loop()
{
    if (complete) {
        
        // Data is available, print via Serial
        
        for (uint16_t i = 0; i < NEVENTS; ++i)
        {
            // Relative time in us relative to first event.
            Serial.print(scope.timeOf(i)); Serial.print(",");            
            // Type of event 1 = rising edge, 0 = falling edge.
            Serial.print(scope.eventOf(i));

            if (i < NEVENTS - 1)
                Serial.print(" ");
            Serial.println();
        }
        
        // Restart the scope after a short pause.
        delay(5000);
        Serial.println("Ready for capture");
        complete = false;
        scope.start();
    }
}

/**
    Callback function from DigitalScope enough events have been gathered.

    This function is handed to scope through DigitalScope::setCompletedCallback in 
    setup(), before the scope is started. 

    Note, this function is invoked from an interrupt service routine (ISR), therefore
    delay and delayMicroseconds will not work in here. As a rule of thumb, just toggle
    some flags in here that signal loop() what todo.  
    
    Read more about interrupts.
    https://www.arduino.cc/en/Reference/AttachInterrupt
*/
void onComplete() 
{
    // Signal loop() that data is available.
    complete = true;
}