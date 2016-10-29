/**
    This file is part of DigitalScope - a software for capturing digital input events.

    Copyright(C) 2016 Christoph Heindl

    All rights reserved.
    This software may be modified and distributed under the terms
    of the BSD license.See the LICENSE file for details.

    This example collects a any number of samples during a 1 second period after
    a start trigger was encountered. Once the collection is complete it prints 
    the results via Serial. 

    DigitalScope uses carefully designed interrupt service routines to maximize
    the frequency of observable event changes.
*/

#include "Arduino.h"

// Include library
#include <DigitalScope.h>

// Initalize scope with max number of events to collect (128) and target pin (2)
typedef scopes::DigitalScope<256, 2> Scope;
Scope scope;

// Will be used to signal begin of event detection. 
volatile bool started = false;

void setup()
{
    started = false;

    Serial.begin(9600);
    while(!Serial) {}

    // Set our callback function when data recording has begun. 
    scope.setBeginCallback(onBegin);

    // Start recording when we observe a falling edge on input.
    scope.start(FALLING);
    Serial.println("LOG Ready for capture");
}

void loop()
{
    if (started) {
        // Sleep for a second. Scope will continue to record data meanwhile.
        delay(1000);
        
        // Explicitly stop scope
        scope.stop();

        Serial.print("DATA ");
        for (int16_t i = 0; i < scope.numEvents(); ++i)
        {
            // Print info about event time in microseconds 
            // since first event and the current state HIGH/LOW
            Serial.print(scope.timeOf(i)); Serial.print(" ");            
            Serial.print(scope.stateOf(i)); Serial.print(" ");
        }
        Serial.println();

        // Restart the scope after a short pause.

        delay(3000);
        Serial.println("LOG Ready for capture");
        started = false;
        scope.start(FALLING);
    }
}

/**
    Callback function invoked by scope when first sample was recorded.

    This function is handed to scope through DigitalScope::setBeginCallback in 
    setup(), before the scope is started.     
*/
void onBegin() 
{
    // Signal loop() that first event was recorded.
    started = true;
}