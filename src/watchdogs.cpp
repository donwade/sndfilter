/* Task_Watchdog Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "watchdogs.h"

extern "C" unsigned long millis(void);

/*
    Effectively create a low priority task on each core. If the low priority
    task starves, the dog which is attached to it will trigger a WDT reset
*/

#define TWDT_DOG_TIMER_SEC    3

void setup_watchdogs(void)
{

	// Configure the Task Watchdog Timer
	esp_task_wdt_config_t twdt_config = {
		.timeout_ms = 5000, // Set timeout to 5 seconds
		.idle_core_mask = (1 << configNUM_CORES) - 1, // Monitor all cores' idle tasks
		.trigger_panic = true, // Trigger a panic (and reboot) on timeout
	};

    // Initialize the TWDT with the specified configuration
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));

}

//-------------------------------------------------------------------------

// CORE 0 has WDT DISABLED when the RTOS was built
// so don't bother putting anything on 0 or it will crash.

#define DEFAULT_CORE 1   

typedef struct dogTaskData
{ 
	TaskFunction_t pvTaskCode;
	void *pvParameters;
	uint32_t stackSize;
	char name[20];
};

//-------------------------------------------
// allow long delays past watchdog
void Tdelay(unsigned int ms)
{
	//printf("\n%s ms=%d\n", __FUNCTION__, ms);	
	while (ms  > TWDT_DOG_TIMER_SEC * 1000)
	{
		kickDog();
		delay(TWDT_DOG_TIMER_SEC * 1000 - 1);
		ms -= TWDT_DOG_TIMER_SEC * 1000;
	}

	kickDog();
	if (ms > 0) delay(ms);
}

//-------------------------------------------

//#define PROFILE_DOG

#include "freertos/FreeRTOS.h"

void onEntryDog(void * const inParam)
{
	{
	    //	Add this task to TWDT
	    //  Only being inside thread can do this.
	    //	Then see if it the RTOS kept it.

	    // put this thread under control of the WDT thread
	    ABORT_ON_FAIL(esp_task_wdt_add(NULL), ESP_OK);

	    // did it stick?
	    ABORT_ON_FAIL(esp_task_wdt_status(NULL), ESP_OK);

 	} // this has to be done before any dog kicks !

	dogTaskData *setup = (dogTaskData *) inParam;

	delay(10);
    uint32_t freeStack;

	kickDog();

#ifdef PROFILE_DOG
	unsigned long ms=millis();
    unsigned long now;
    unsigned long diff;
    printf("*********** START TIME = %d\n", now);
#endif 

	// never ending call loop.
    while(1)
    {
    
#ifdef PROFILE_DOG
        now=millis();
        diff = now - ms;

        ms = now;
        printf("***** %s dog time = %d mS\n", setup->name, diff);
#endif

		setup->pvTaskCode(setup->pvParameters);
		
		delay(1);

    }
}
//-------------------------------------------------------------
TaskHandle_t spawnTaskAndDogV2(  TaskFunction_t pvTaskCode,
                                const char * const pcName,
                                const uint32_t usStackDepth,
                                void * const pvParameters,
                                UBaseType_t uxPriority)
{
    int tskParam;
    TaskHandle_t retval;

    //Initialize WDT, doing it again will cause a crash

	printf("%s creating %s\n", __FUNCTION__, pcName);

	dogTaskData *passIn = (dogTaskData*) malloc(sizeof(dogTaskData));

	passIn->pvParameters = pvParameters;
	passIn->pvTaskCode = pvTaskCode;
	passIn->stackSize = usStackDepth;
	strncpy(passIn->name, pcName, sizeof(passIn->name));
	passIn->name[sizeof(passIn->name)-1] = '\0';

    xTaskCreatePinnedToCore(onEntryDog, pcName, usStackDepth, passIn, uxPriority, &retval, DEFAULT_CORE);
    return retval;

}
//----------------------------------------------------------------------------------

void kickDog(void)
{
	ABORT_ON_FAIL(esp_task_wdt_reset(), ESP_OK);
}
//----------------------------------------------------

