#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>

extern int alt_main(int argc, char **argv);

static const gpio_num_t SDCARD_CSPIN = GPIO_NUM_4;

void setup(void)
{
	int i;
	char *msg[] = {"notused.exe", "up.wav", "output.wav", "highpass", "300", "3"};
	i = sizeof(msg) / sizeof(msg[0]);

    //bool ok = SD.begin();
    bool ok = SD.begin(SDCARD_CSPIN, SPI, 10000000);
	delay(1000);
	
	alt_main(i, msg);
	    
}

void loop(void)
{
    delay(1000);
}
