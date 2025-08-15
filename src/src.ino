#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>
#include "wavePlayer.h"
#include "watchdogs.h"

extern void setup_wavePlayer(void);
extern int alt_main(int argc, char **argv);
extern void setup_watchdogs(void);

static const gpio_num_t SDCARD_CSPIN = GPIO_NUM_4;

void runitTask (void *NOTUSED)
{
	int i;
	char *msg[] = {"notused.exe", "/one.wav", "/output.wav", "highpass", "300", "3"};
	i = sizeof(msg) / sizeof(msg[0]);

    Serial.printf("lllllllllllllllllllllllllllllllllll\n");
    Serial.flush();

	alt_main(i, msg);
    Serial.printf("sssssssssssssssssssssssssssssss\n");
    Tdelay(-1);
}

//===================================================================================

void setup(void)
{
    M5.begin();
    Serial.begin(115200);

    //bool ok = SD.begin();
    bool ok = SD.begin(SDCARD_CSPIN, SPI, 1000000);
	delay(1000);
    //setup_watchdogs();
    setup_wavePlayer();

    // loop does not have enough stack size. Do not know how to set it.
    // Create a task and specify task stack size
    
	spawnTaskAndDogV2( runitTask,	//(void * not_used)TaskFunction_t pvTaskCode,
                     "runitTask",   //const char * const pcName,
                     1024 * 30,		//const uint32_t usStackDepth,
                     NULL,			//void * const pvParameters,
                     4           	//UBaseType_t uxPriority)
                     );
	    
}

void loop(void)
{
    delay(1000);
}
