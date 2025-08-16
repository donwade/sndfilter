#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>
#include "wavePlayer.h"
#include "watchdogs.h"

extern void setup_wavePlayer(void);
extern int alt_main(int argc, char **argv);
extern void setup_watchdogs(void);
extern bool add_to_playlist(char *filename);

static const gpio_num_t SDCARD_CSPIN = GPIO_NUM_4;
extern bool playWavFromSD(const char* filename);



void runitTask (void *NOTUSED)
{
	int i;
	char *msg[] = {"notused.exe", "/one.wav", "/output.wav", "lowpass", "300", "3"};
	i = sizeof(msg) / sizeof(msg[0]);

    Serial.printf("lllllllllllllllllllllllllllllllllll\n");
    Serial.flush();

	alt_main(i, msg);
	
	add_to_playlist(msg[2]);
	//playWavFromSD(msg[2]);
	
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
