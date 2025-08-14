#include <SD.h>
#include <M5Unified.h>
#include <esp_log.h>

extern void setup_wavePlayer();
extern void wavPlayerTask(void *NOTUSED);
extern bool add_to_playlist(char *filename);

