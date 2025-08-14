#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"

#if 0
// stop on failure but always report success
#define ABORT_ON_FAIL(functionCall, expected) ({                \
    int retval = functionCall;                                  \
    if(retval != expected){                                     \
        printf("ERROR = %s %d\n", #functionCall, retval);       \
        abort();                                                \
    }                                                           \
    else                                                        \
    {                                                           \
        printf("PASS =  %s %d\n", #functionCall, retval);       \
    }                                                           \
 })
#else

// report and stop only on failure
#define ABORT_ON_FAIL(functionCall, expected) ({                \
    int retval = functionCall;                                  \
    if(retval != expected){                                     \
        printf("ERROR = %s %d\n", #functionCall, retval);       \
        abort();                                                \
    }                                                           \
 })

#endif

extern TaskHandle_t spawnTaskAndDogV2(  TaskFunction_t pvTaskCode, const char * const pcName, const uint32_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority);
extern void kickDog(void);
extern void Tdelay(unsigned int ms);



