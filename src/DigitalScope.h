
/**
    This file is part of DigitalScope - a software scope for capturing digital input events.

    Copyright(C) 2016 Christoph Heindl

    All rights reserved.
    This software may be modified and distributed under the terms
    of the BSD license.See the LICENSE file for details.
*/

#ifndef DIGITAL_SCOPE
#define DIGITAL_SCOPE

#include <Arduino.h>
#include <util/atomic.h>

#ifndef DIGITALSCOPE_NO_RETURN_WARN
    #define DIGITALSCOPE_NO_RETURN_WARN(type) return type(0);
#endif

namespace cheind {

    /** A digital scope to capture edge event changes at input pins.

        ## Introduction

        This implementation of a software scope records digital event changes (HIGH->LOW and LOW->HIGH)
        of digital input pins. DigitalScope supports three different types of start triggers 
        (CHANGE, RISING, FALLING).

        The number of events DigitalScope can record is specified by the template argument `N`. 

        Upon the first event the scope records the absolute timestamp in microseconds 
        (DigitalScope::timeOfStart) and makes subsequent events relative to reference timestamp. 
        DigitalScope::timeOf reports the relative time of an indexed event. DigitalScope::eventOf
        reports the reconstructed edge event (RISING, FALLING) for an indexed event and 
        DigitalScope::stateOf returns the associated state (HIGH,LOW). 

        The template argument `D` allows you to specify the data depth of each 
        timestamp. Using the default (uint32_t) allows for correct recording of event timestamps of
        up to 2^32 microseconds (~ 4296 seconds) after the first event was captured. You may choose less
        bits to save memory at the expense of shorter capture periods. DigitalScope does not perform
        any overflow checks.

        See `examples/` directory for example usage.

        ## Notes

        The digital scope uses an interrupt service routine (ISR) to capture event changes on target pins. 
        Not all boards support ISRs to be attached to all available pins. Refer to 
        https://www.arduino.cc/en/Reference/AttachInterrupt to determine which pins or pin-groups are supported.

        ## Warnings
        
        When using callbacks such as DigitalScope::onComplete, DigitalScope::onFirst take notice that
        those callbacks are invoked from an interrupt service handler (ISR). Make sure that your callback
        is as short as possible to avoid missing any events. Also, functions such as delay will not work
        correctly from within ISRs.

        DigitalScope can only be configured for a single input pin and for peculiarities of ISRs only 
        one DigitalScope should be running at any point in time.
        
        Read more about ISRs: https://www.arduino.cc/en/Reference/AttachInterrupt 
    **/

    template<uint16_t N, typename D = uint32_t>
    class DigitalScope {      
    public:

        typedef void (*CallbackFnc)();

        /** Initialize a scope using the pin to read events from. */
        DigitalScope(uint8_t pin)
        : _pin(pin)
        {
            pinMode(_pin, INPUT);
            attachInterrupt(digitalPinToInterrupt(_pin), DigitalScope<N>::onChange, CHANGE);
        }

        /** Ensure detaching of ISR when deallocating object. */
        ~DigitalScope()
        { 
            detachInterrupt(digitalPinToInterrupt(_pin));
        }

        /** Set a callback function to be invoked when desired number of events were recorded.

            \param fnc  a pointer to function with signature void(void). Pass zero pointer to
                        disable a previously set callback.     
        */
        void setCompletedCallback(CallbackFnc fnc) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (!_data.enabled) {
                    _data.onComplete = fnc;
                }
            }
        }

        /** Set a callback function to be invoked after the first event was recorded.
            
            \param fnc  a pointer to function with signature void(void). Pass zero pointer to
            disable a previously set callback.   
        */
        void setBeginCallback(CallbackFnc fnc) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (!_data.enabled) {
                    _data.onBegin = fnc;
                }
            }
        }

        /** Start event collection.

            Recording starts when specified trigger mode is encountered. Currently,
            there are trigger modes are supported:
                - CHANGE: Trigger when encountering the first edge change
                - RISING: Trigger when encountering a rising egde
                - FALLING: Trigger when encounering a falling edge

            \param triggerMode Trigger mode to start event capturing. 
         */
        void start(uint8_t triggerMode = CHANGE) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (_data.enabled) return;

                self = this; 

                _data.start = 0;
                uint8_t state = digitalRead(_pin);

                switch (triggerMode) {
                case CHANGE: 
                    _data.idx = 0;
                    _istate = state; 
                    break;
                case RISING:
                    _data.idx = (state == LOW) ? 0 : -1;
                    _istate = HIGH; 
                    break;
                case FALLING:
                    _data.idx = (state == HIGH) ? 0 : -1;
                    _istate = LOW; 
                    break;
                }

                _data.enabled = true;                
            }
        }

        /** Stop event collection. */
        void stop() {
             ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                 _data.enabled = false;
             }             
        }

        /** Test if scope is currently collecting events. */
        bool isEnabled() const {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                return _data.enabled;
            }    
            DIGITALSCOPE_NO_RETURN_WARN(bool);
        }

        /** Number of events already collected. */
        uint16_t numEvents() const {
            int32_t idx;
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                idx = _data.idx; 
            }
            return idx < 0 ? 0 : static_cast<uint16_t>(idx);
        }

        /** Test if the number of events collected matches internal buffer size. */
        bool completed() const {
            return numEvents() == N;
        }

        /** Returns the event time (us) relative to start time of event collection. */ 
        uint32_t timeOf(uint16_t idx) const {
            return _data.samples[idx];
        } 

        /** Return the event type associated with a specific event.

            \return RISING or FALLING depending on the event type.
         */
        uint8_t eventOf(uint16_t idx) const {
            const bool isFalling = (idx % 2 == 0) ^ (_istate == HIGH); 
            return isFalling ? FALLING : RISING;
        }

        /** Return the state type associated with a specific event.

            \return HIGH or LOW depending on the state.
        */
        uint8_t stateOf(uint16_t idx) const {
            return eventOf(idx) == RISING ? HIGH : LOW;            
        }

        /** Returns event collection start time (us) since the Arduino began running the current program. */
        const uint32_t timeOfStart() const {
            return _data.start;
        }

        /** Returns the initial state of input just before sampling started. */
        uint8_t initialState() const {
            return _istate;
        }

    private:

        struct SharedData {
            int32_t idx;
            uint32_t start;
            D samples[N];
            CallbackFnc onBegin;
            CallbackFnc onComplete;
            bool enabled;

            SharedData()
                :idx(0), start(0), onBegin(0), onComplete(0), enabled(false)
            {}
        };

        static DigitalScope<N, D> *self;
        
        /** ISR - Called per edge change._buffer

            Updates state of DigitalScope<T>::SharedData.
         */
        static void onChange() {
            uint32_t now = micros();

            volatile SharedData &d = self->_data;
            
            // When not enabled, do nothing
            if (!d.enabled) {
                return;
            } 
            
            // The idx variable tells us about the next event slot. 
            // Here we have the following possibilities: 
            
            if (d.idx < 0) {
                // A negative index tells us to skip this event (trigger).
                d.idx++;
            } else if (d.idx == N) {
                // We have reached the maximum number of events we can store.
                d.enabled = false;
                if (d.onComplete) d.onComplete();
            } else {                
                if (d.idx == 0) {
                    // On first event we record start time.   
                    d.start = now;
                    if (d.onBegin) d.onBegin();
                }
                
                // Save timestamp
                d.samples[d.idx++] = D(now - d.start);
            }
            
        }

        uint8_t _pin;
        uint8_t _istate;
        volatile SharedData _data;
    };

    template <uint16_t N, typename D>
    DigitalScope<N, D> *DigitalScope<N, D>::self = 0;
}

#endif