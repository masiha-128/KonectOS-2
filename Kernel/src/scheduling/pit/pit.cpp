#include "pit.h"

namespace PIT{
    double TimeSinceBoot = 0;
	double TimeUpdate = 0;
	double TimeBetweenTwoImages = 0;

    uint16_t Divisor = uint16_Limit;

    void Sleepd(double seconds){
        double startTime = TimeSinceBoot;
        double endTime = startTime + seconds;
        while(true){
            if((double)TimeSinceBoot >= (double)endTime){
                break;
            }
            asm("hlt");
        }
    }

    void Sleep(uint64_t milliSeconds){
        Sleepd((double)milliSeconds / 1000);
    }

    void SetDivisor(uint16_t divisor){
		//Calc FPS
        if(divisor < 100) divisor = 100;
        Divisor = divisor;
        IoWrite8(0x40, (uint8_t)(divisor & 0x00ff));
        io_wait();
        IoWrite8(0x40, (uint8_t)(divisor & 0xff00) >> 8);
    }

    uint64_t GetFrequency(){
        return BaseFrequency / Divisor;
    }

    void SetFrequency(uint64_t frequency){
        SetDivisor(BaseFrequency / frequency);
    }

    void Tick(){  
        TimeSinceBoot += (double) 1 / (double)GetFrequency();
        TimeUpdate += (double) 1 / (double)GetFrequency();

		if(TimeUpdate >= 0.06){
			globalGraphics->Update();
			TimeUpdate = 0;
		}
    }
}