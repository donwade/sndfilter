#include <SD.h>
#include <M5Unified.h>
#include "wavePlayer.h"
//#include "watchdogs.h"
#include <cppQueue.h>

#include <esp_log.h>

#define LINE Serial.printf("%s:%d\n", __FUNCTION__, __LINE__)

static void loadWavFiles(File dir, int numTabs);
static bool hasWavFileExt(char* filename);


static constexpr const size_t buf_num = 3;
static constexpr const size_t buf_size = 1024 * 16;
static SemaphoreHandle_t xCountingSemaphore;


static File root;

#define MAX_FILES_CACHED 200

#define MAX_FILENAME_LEN 50
#define MAX_FILES_QUEUED 30


static char *wavList[MAX_FILES_CACHED] = {0};

static int numActiveSndFiles = 0;


static uint8_t wav_data[buf_num][buf_size];

struct __attribute__((packed)) wav_header_t
{
  char RIFF[4];
  uint32_t chunk_size;
  char WAVEfmt[8];
  uint32_t fmt_chunk_size;
  uint16_t audiofmt;
  uint16_t channel;
  uint32_t sample_rate;
  uint32_t byte_per_sec;
  uint16_t block_size;
  uint16_t bit_per_sample;
};

struct __attribute__((packed)) sub_chunk_t
{
  char identifier[4];
  uint32_t chunk_size;
  uint8_t data[1];
};

//------------------------------------------------

bool playWavFromSD(const char* filename)
{
	char fname[80];
	strcpy(&fname[1], filename);
	fname[0]='/';

	auto file = SD.open(fname);

	if (!file) { return false; }

	wav_header_t wav_header;
	file.read((uint8_t*)&wav_header, sizeof(wav_header_t));

	Serial.printf("RIFF           : %.4s\n" , wav_header.RIFF          );
	Serial.printf("chunk_size     : %d\n"   , wav_header.chunk_size    );
	Serial.printf("WAVEfmt        : %.8s\n" , wav_header.WAVEfmt       );
	Serial.printf("fmt_chunk_size : %d\n"   , wav_header.fmt_chunk_size);
	Serial.printf("audiofmt       : %d\n"   , wav_header.audiofmt      );
	Serial.printf("channel        : %d\n"   , wav_header.channel       );
	Serial.printf("sample_rate    : %d\n"   , wav_header.sample_rate   );
	Serial.printf("byte_per_sec   : %d\n"   , wav_header.byte_per_sec  );
	Serial.printf("block_size     : %d\n"   , wav_header.block_size    );
	Serial.printf("bit_per_sample : %d\n"   , wav_header.bit_per_sample);

	if ( memcmp(wav_header.RIFF,    "RIFF",     4)
	|| memcmp(wav_header.WAVEfmt, "WAVEfmt ", 8)
	|| wav_header.audiofmt != 1
	|| wav_header.bit_per_sample < 8
	|| wav_header.bit_per_sample > 16
	|| wav_header.channel == 0
	|| wav_header.channel > 2
	)
	{
		Serial.printf("%s wrong wav format ... rejected\n", fname); 
		file.close();
		return false;
	}

	file.seek(offsetof(wav_header_t, audiofmt) + wav_header.fmt_chunk_size);
	sub_chunk_t sub_chunk;

	file.read((uint8_t*)&sub_chunk, 8);

	ESP_LOGD("wav", "sub id         : %.4s" , sub_chunk.identifier);
	ESP_LOGD("wav", "sub chunk_size : %d"   , sub_chunk.chunk_size);

	while(memcmp(sub_chunk.identifier, "data", 4))
	{
		if (!file.seek(sub_chunk.chunk_size, SeekMode::SeekCur)) { break; }
		file.read((uint8_t*)&sub_chunk, 8);

		ESP_LOGD("wav", "sub id         : %.4s" , sub_chunk.identifier);
		ESP_LOGD("wav", "sub chunk_size : %d"   , sub_chunk.chunk_size);
	}

	if (memcmp(sub_chunk.identifier, "data", 4))
	{
		file.close();
		return false;
	}

	int32_t data_len = sub_chunk.chunk_size;
	bool flg_16bit = (wav_header.bit_per_sample >> 4);

	size_t idx = 0;
	while (data_len > 0) 
	{
		
		size_t len = data_len < buf_size ? data_len : buf_size;
		Serial.printf("len=%d 0x%X\n", len, len);

		len = file.read(wav_data[idx], len);
		data_len -= len;

		if (flg_16bit) 
		{
			M5.Speaker.playRaw((const int16_t*)wav_data[idx], len >> 1, wav_header.sample_rate, wav_header.channel > 1, 1, 0);
		}
		else 
		{
			M5.Speaker.playRaw((const uint8_t*)wav_data[idx], len, wav_header.sample_rate, wav_header.channel > 1, 1, 0);
		}
			idx = idx < (buf_num - 1) ? idx + 1 : 0;
	}
  
	file.close();

  return true;
}

//------------------------------------------------

void setup_wavePlayer()
{

	/* useful search words 
	  board_M5StackCore2
	*/

	//SD.begin(SDCARD_CSPIN, SPI, 25000000);

	M5.Speaker.setVolume(128);

	root = SD.open("/");

	// find number of wave files
	loadWavFiles(root, 0); 

	int maxFileEntries = (sizeof(wavList) / sizeof(wavList[0]));
	Serial.print("maxFileEntries: ");
	Serial.println(maxFileEntries);

	Serial.print("numActiveSndFiles: ");
	Serial.println(numActiveSndFiles);

	Serial.println("done!");

	xCountingSemaphore = xSemaphoreCreateCounting(MAX_FILES_QUEUED,0);

}

//------------------------------------------------
static void loadWavFiles(File dir, int numTabs) {

  while(true)
  {

	if (MAX_FILES_CACHED == numActiveSndFiles)
	{
		Serial.printf("limited to %d files\n", numActiveSndFiles);
		break;
	}
	
    File entry =  dir.openNextFile();
    if (! entry) 
	{
      // no more files
      break;
    }
	
    for (uint8_t i=0; i<numTabs; i++)
	{
      Serial.print('\t');
    }

    String file_name = entry.name();

	unsigned int len = strlen(entry.name()) + 1;
	assert(len < MAX_FILENAME_LEN);
	
	char *tempc = (char*) malloc(len);
	strcpy(tempc, entry.name());
	
    if ( hasWavFileExt(tempc) && file_name.indexOf('_') != 0 )
	{ // Here is the magic
	
		Serial.print(numActiveSndFiles); 
		Serial.print(" "); 

		wavList[numActiveSndFiles] = tempc; //add to array

		Serial.println(wavList[numActiveSndFiles]); //show array item
		numActiveSndFiles++;
    }

#if 0
    if (entry.isDirectory()) 
	{ 
		// Dir's will print regardless, you may want to exclude these
      	//Serial.print(entry.name());
      	//Serial.println("/");
     	//loadWavFiles(entry, numTabs+1);
    } else {
      	// files have sizes, directories do not
      	//Serial.print("\t\t");
      	//Serial.println(entry.size(), DEC);
    }
#endif

    entry.close();
    
  }
}

//------------------------------------------------

// note strlwr writes into src, so filename cant be const data ?

static bool hasWavFileExt(char* filename) 
{
	int8_t len = strlen(filename);
	bool result;

	if (strstr(strlwr(filename + (len - 4)), ".wav"))
	{
		result = true;
	} else
	{
		result = false;
	}
	
	return result;
}

#if 0
	|| strstr(strlwr(filename + (len - 4)), ".mp3")
	|| strstr(strlwr(filename + (len - 4)), ".aac")
	|| strstr(strlwr(filename + (len - 4)), ".wma")
	|| strstr(strlwr(filename + (len - 4)), ".fla")
	|| strstr(strlwr(filename + (len - 4)), ".mid")
	|| strstr(strlwr(filename + (len - 4)), ".raw")
	// and anything else you want
#endif
	
	

