
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
        
        Serial.println("BEGIN DATA");
        for (uint16_t i = 0; i < NEVENTS; ++i)
        {
            // Print info about event time in microseconds 
            // since first event, the current state HIGH/LOW, the event type triggering
            // the state change RISING/FALLING.
            Serial.print(scope.timeOf(i)); Serial.print(",");            
            Serial.print(scope.stateOf(i)); Serial.print(",");
            Serial.print(scope.eventOf(i));
            Serial.println();
        }
        Serial.println("END DATA");
        
        // Restart the scope after a short pause.
        delay(5000);
        Serial.println("LOG Ready for capture");
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