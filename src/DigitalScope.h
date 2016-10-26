
/**
    This file is part of ScopeOne - a software scope for capturing digital input events.

    Copyright(C) 2015/2016 Christoph Heindl

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

    /** A digital scope to capture edge event changes on input pins.

        This implementation of a software scope records digital event changes (HIGH->LOW and LOW->HIGH)
        on digital input pins. A DigitalScope can only be configured for a single input pin and for
        specialities of ISRs only one DigitalScope should be enabled at any point in time.

        The DigitalScope records timepoints of edge events. The number of events it can record in 
        its internal buffer is specified by the template argument N. Since the scope only records
        edge events, it will only track timestamps of events and reconstruct the serials of events
        from an initial state and event number.

        Upon the first event the scope records the absolute timestamp in microseconds 
        (DigitalScope::timeOfStart) and makes subsequent events relative to reference timestamp. 
        DigitalScope::timeOf reports the relative time of an indexed event. DigitalScope::eventOf
        reports the reconstructed edge event for an indexed event.

        Note:
        The digital scope uses an interrupt service routine (ISR) to capture
        event changes on target pins. Not all boards support ISRs to be attached
        to all available pins. Tefer to https://www.arduino.cc/en/Reference/AttachInterrupt to determine
        which pins or pin-groups are supported.

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

        /** Set a callback function to be invoked when desired number of events have been gathered. */
        void setCompletedCallback(CallbackFnc fnc) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (!_data.enabled) {
                    _data.onComplete = fnc;
                }
            }
        }

        /** Set a callback function to be invoked after the first event was recorded. */
        void setFirstCallback(CallbackFnc fnc) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (!_data.enabled) {
                    _data.onFirst = fnc;
                }
            }
        }

        /** Start event collection. */
        void start() {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                if (_data.enabled) return;

                self = this; 

                _data.idx = 0;
                _data.start = 0;                                
                _initialState = digitalRead(_pin);
               
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
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                return _data.idx;
            }
            DIGITALSCOPE_NO_RETURN_WARN(uint16_t);
        }

        /** Test if the number of events collected matches internal buffer size. */
        bool completed() const {
            return numEvents() == N;
        }

        /** Returns the event time (us) relative to start time of event collection. */ 
        D timeOf(uint16_t idx) const {
            return _data.samples[idx];
        } 

        /** Return the event type associated with a specific event. */
        uint8_t eventOf(uint16_t idx) const {
            // State toggles with each event and is dependent on initial state.
            bool even = (idx % 2 == 0);
            return even ? !_initialState : _initialState;
        }

        /** Returns event collection start time (us) since the Arduino began running the current program. */
        const uint64_t timeOfStart() const {
            return _data.start;
        }

        /** Returns the initial state of input just before sampling started. */
        uint8_t initialState() const {
            return _initialState;
        }

    private:

        struct SharedData {
            uint16_t idx;
            uint64_t start;
            D samples[N];
            CallbackFnc onFirst;
            CallbackFnc onComplete;
            bool enabled;

            SharedData()
                :idx(0), start(0), onFirst(0), onComplete(0), enabled(false)
            {}
        };

        static DigitalScope<N, D> *self;
        
        /** ISR - Called per edge change._buffer

            Updates state of DigitalScope<T>::SharedData.
         */
        static void onChange() {
            uint64_t now = micros();

            volatile SharedData &d = self->_data;

            if (!d.enabled) {
                return;
            } else if (d.idx == N) {
                d.enabled = false;
                if (d.onComplete) d.onComplete();
                return;
            } else if (d.idx == 0) {
                d.start = now;
                if (d.onFirst) d.onFirst();                
            }

            d.samples[d.idx++] = D(now - d.start);
        }

        uint8_t _pin;
        uint8_t _initialState;
        volatile SharedData _data;
    };

    template <uint16_t N, typename D>
    DigitalScope<N, D> *DigitalScope<N, D>::self = 0;
}

#endif