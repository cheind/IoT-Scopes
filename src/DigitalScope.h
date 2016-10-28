
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

namespace scopes {

    /** Enable callbacks invoked when first event is observed. */
    const int OptionBeginCallback = 1 << 1;

    /** Enable callbacks invoked when all samples are recorded. */
    const int OptionCompleteCallback = 1 << 2;

    /** Enable automatically stopping the capture process once no more space is left. */
    const int OptionAutoStop = 1 << 3;

    /** Default options. */
    const int DefaultOptions = OptionBeginCallback | OptionCompleteCallback | OptionAutoStop;

    /** A digital scope to capture edge event changes at input pins.

        ## Introduction

        This implementation of a software scope records digital event changes (HIGH->LOW and LOW->HIGH)
        of digital input pins. DigitalScope supports three different types of start triggers 
        (CHANGE, RISING, FALLING).

        ## Template Arguments
        
        \tparam N_ Number of events to capture. Needs to be a power of 2
        \tparam Pin_ Index of pin to read from
        \tparam Options_ A bitwise or'ed combination of compile time options. Defaults to  
                         OptionBeginCallback | OptionCompleteCallback | OptionAutoStop;

                        
        ## What is recorded        
        
        Upon the first event the scope records the absolute timestamp in microseconds 
        (DigitalScope::timeOfStart) and makes subsequent events relative to this timestamp. 
        DigitalScope::timeOf reports the relative time of an indexed event. DigitalScope::eventOf
        reports the reconstructed edge event (RISING, FALLING) for an indexed event and 
        DigitalScope::stateOf returns the associated state (HIGH,LOW). 

        ## Triggering

        By default recording starts when the first change event is detected after DigitalScope::start
        was called. Besides CHANGING, RISING and FALLING triggers are supported.

        See `examples/` directory for example usage.
        
        ## Notes

        The digital scope uses an interrupt service routine (ISR) to capture event changes on target pins. 
        Not all boards support ISRs to be attached to all available pins. Refer to 
        https://www.arduino.cc/en/Reference/AttachInterrupt to determine which pins or pin-groups are supported.

        When using callbacks such as DigitalScope::onComplete, DigitalScope::onBegin take notice that
        those callbacks are invoked from an interrupt service handler (ISR). Make sure that your callback
        is as short as possible to avoid missing any events. Also, functions such as delay will not work
        correctly from within ISRs.

        Read more about ISRs: https://www.arduino.cc/en/Reference/AttachInterrupt

        ## Warning

        DigitalScope can only be configured for a single input pin and for peculiarities of ISRs only 
        one DigitalScope should be running at any point in time.
    **/
    template<int16_t N_, uint8_t Pin_, int Options_ = DefaultOptions>
    class DigitalScope {      
    public:
        enum {
            N = N_,
            Pin = Pin_,
            IsPowerOfTwo = N && !(N & (N - 1)),
            WithBeginCallback = (Options_ & OptionBeginCallback),
            WithCompleteCallback = (Options_ & OptionCompleteCallback),
            WithAutoStop = (Options_ & OptionAutoStop)
        };

        static_assert(IsPowerOfTwo, "N needs to be a power of two i.e 128, 256, 521");

        typedef void (*CallbackFnc)();

        /** Initialize a scope */
        DigitalScope()
        {}

        /** Ensure detaching of ISR when deallocating object. */
        ~DigitalScope()
        { 
            stop();
        }

        /** Set a callback function to be invoked when desired number of events were recorded.

            A callback function will only be invoked when support for it has been enabled at 
            compile time through the Options_ template argument.

            \param fnc  a pointer to function with signature void(void). Pass emptyCallback() to
                        disable a previously set callback.     
        */
        void setCompleteCallback(CallbackFnc fnc) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                _data.onComplete = fnc;
            }
        }

        /** Set a callback function to be invoked after the first event was recorded.

            A callback function will only be invoked when support for it has been enabled at 
            compile time through the Options_ template argument.
            
            \param fnc  a pointer to function with signature void(void). Pass emptyCallback() to
            disable a previously set callback.   
        */
        void setBeginCallback(CallbackFnc fnc) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                _data.onBegin = fnc;
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
                self = this;

                pinMode(Pin_, INPUT);
                uint8_t state = digitalRead(Pin);

                switch (triggerMode) {
                case CHANGE:
                    _data.idx = 0;
                    _istate = !state;
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

                attachInterrupt(digitalPinToInterrupt(Pin_), _data.idx < 0 ? Self::onChangeSkipFirstN : Self::onChange, CHANGE);
                EIFR |= 0x01; // clear any pending interrupts from before start().          
            }
        }

        /** Stop event collection. */
        void stop() {
             ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                 detachInterrupt(digitalPinToInterrupt(Pin_));
             }             
        }

        /** Number of events already collected. */
        int16_t numEvents() const {
            int16_t idx;
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                idx = _data.idx;
            }            
            return idx > 0 ? idx : 0; 
        }

        /** Test if the number of events collected matches internal buffer size. */
        bool completed() const {
            return numEvents() == N;
        }

        /** Returns the event time (us) relative to start time of event collection.

            Should only be called after DigitalScope::stop().         
        */ 
        uint32_t timeOf(int16_t idx) const {
            return _data.samples[idx] - timeOfStart();
        } 

        /** Return the event type associated with a specific event.

            \return RISING or FALLING depending on the event type.
         */
        uint8_t eventOf(int16_t idx) const {
            const bool isFalling = (idx % 2 == 0) ^ (_istate == HIGH); 
            return isFalling ? FALLING : RISING;
        }

        /** Return the state type associated with a specific event.

            \return HIGH or LOW depending on the state.
        */
        uint8_t stateOf(int16_t idx) const {
            return eventOf(idx) == RISING ? HIGH : LOW;            
        }

        /** Returns trigger time (us) since the Arduino began running the current program.

            Should only be called after DigitalScope::stop().  
        */
        const uint32_t timeOfStart() const {
            return _data.samples[0];
        }

        /** Returns the initial state of input just before sampling started. */
        uint8_t initialState() const {
            return _istate;
        }

        /** Empty callback method prototype. */
        static void emptyCallback() {}

    private:

        typedef DigitalScope<N_, Pin_, Options_> Self;

        struct SharedData {
            volatile int16_t idx;
            uint32_t samples[N];
            volatile CallbackFnc onBegin;
            volatile CallbackFnc onComplete;

            SharedData()
                :idx(0), onBegin(Self::emptyCallback), onComplete(Self::emptyCallback)
            {}
        };

        static Self *self;      
        
        inline static void onChange() {
            uint32_t now = micros();
            SharedData &d = self->_data;

            Self::_onChange(&now, d);           
        }

        inline static void onChangeSkipFirstN() {
            uint32_t now = micros();
            SharedData &d = self->_data;

            int16_t idx = d.idx;
            if (idx < 0) {
                d.idx = idx + 1;
                return;
            }

            Self::_onChange(&now, d);                   
        }

        inline static void _onChange(uint32_t *now, SharedData &d) {
            

            int16_t idx = d.idx;            // Read in volatile here once.
            
            int16_t slot = idx & (N - 1);   // Fast modulo for power of two numbers.
            d.samples[slot] = *now;

            if (WithBeginCallback)          // Compile time if
            {
                if (idx == 0)
                {
                    d.onBegin();
                }
            }

            idx++;
            d.idx = idx;

            if (WithCompleteCallback | WithAutoStop) // Compile time if
            {
                const bool complete = (idx == N_);

                if (complete)
                {
                    if (WithAutoStop) // Compile time if
                        detachInterrupt(digitalPinToInterrupt(Pin_));
                    if (WithCompleteCallback) // Compile time if
                        d.onComplete();
                }
            }
        }

        uint8_t _istate;
        SharedData _data;
    };

    template <int16_t N, uint8_t P, int O>
    DigitalScope<N, P, O> *DigitalScope<N, P, O>::self = 0;
}

#endif