#if !defined(MQTT_MBED_H)
#define MQTT_MBED_H

#include "mbed-drivers/mbed.h"

class Countdown
{
    public:
        Countdown()
        {
        }

        Countdown(int ms)
        {
            countdown_ms(ms);   
        }

        bool expired()
        {
            return t.read_ms() >= interval_end_ms;
        }

        void countdown_ms(unsigned long ms)  
        {
            t.stop();
            interval_end_ms = ms;
            t.reset();
            t.start();
        }

        void countdown(int seconds)
        {
            countdown_ms((unsigned long)seconds * 1000L);
        }

        int left_ms()
        {
            return interval_end_ms - t.read_ms();
        }

    private:
        Timer t;
        int interval_end_ms; 
};

class DummyThread
{
	public:
	DummyThread(void (*)(void const *argument), void* arg){
		(void)arg;
	}

};

class DummyMutex
{
	

};

#endif
