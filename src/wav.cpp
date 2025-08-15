//
// sndfilter - Algorithms for sound filters, like reverb, lowpass, etc
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/sndfilter
// SPDX-License-Identifier: 0BSD
//

#include "wav.h"
#include <stdio.h>
#include <stdint.h>
#include <SPI.h>
#include <SD.h>


#define LINE Serial.printf("%s:%d \n", __FUNCTION__, __LINE__)
#define VLINE(val) Serial.printf("%s:%d "#val"= 0x%X (%d) \n", __FUNCTION__, __LINE__, val, val)
#define V2LINE(val,cal) Serial.printf("%s:%d "#val"= 0x%X (%d) expect 0x%X (%d) \n",\
	__FUNCTION__, __LINE__,\
	val, val,\
	cal, cal)


sf_snd sf_randomload(const char *notused);


// read an unsigned 32-bit integer in little endian format
static inline uint32_t read_u32le(File fp){
	uint32_t b1 = fp.read();
	uint32_t b2 = fp.read();
	uint32_t b3 = fp.read();
	uint32_t b4 = fp.read();
	return b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
}

// read an unsigned 16-bit integer in little endian format
static inline uint16_t read_u16le(File fp){
	uint16_t b1 = fp.read();
	uint16_t b2 = fp.read();
	return b1 | (b2 << 8);
}

// write an unsigned 32-bit integer in little endian format
static inline void write_u32le(File fp, uint32_t v){
	fp.write(v & 0xFF);
	fp.write((v >> 8) & 0xFF);
	fp.write((v >> 16) & 0xFF);
	fp.write((v >> 24) & 0xFF);
}	

// write an unsigned 16-bit integer in little endian format
static inline void write_u16le(File fp, uint16_t v){
	fp.write(v & 0xFF);
	fp.write((v >> 8) & 0xFF);
}

// load a WAV file (returns NULL for error)
sf_snd sf_wavload(const char *file){

	
	File fp;

	fp = SD.open(file);
	//FILE *fp = fopen(file, "rb");
	LINE;
	
	if (!fp)
	{
		Serial.printf("file not found %s\n", file);
		return NULL;
	}

	LINE;
	uint32_t riff = read_u32le(fp);
	if (riff != 0x46464952){ // 'RIFF'
		V2LINE(riff, 0x46464952);
		fp.close();
		return NULL;
	}

	V2LINE(riff, 0x46464952);
	read_u32le(fp); // filesize; don't really care about this

	uint32_t wave = read_u32le(fp);
	V2LINE(wave, 0x45564157);
	
	if (wave != 0x45564157)
	{ // 'WAVE'
		LINE;
		fp.close();
		return NULL;
	}
	LINE;
	
	// start reading chunks
	bool found_fmt = false;
	uint16_t audioformat;
	uint16_t numchannels;
	uint32_t samplerate;
	uint16_t bps;

	while (fp.available())
	{
		uint32_t chunkid = read_u32le(fp);
		uint32_t chunksize = read_u32le(fp);
		
		V2LINE(chunkid, 0x20746D66);
		VLINE(chunksize);
		
		if (chunkid == 0x20746D66)
		{ // 'fmt '
			// confirm we haven't already processed the fmt chunk, and that it's a good size
			if (found_fmt || chunksize < 16)
			{
				LINE;
				fp.close();
				return NULL;
			}

			found_fmt = true;

			// load the fmt information
			audioformat = read_u16le(fp);
			VLINE(audioformat);
			
			numchannels = read_u16le(fp);
			VLINE(numchannels);
			
			samplerate  = read_u32le(fp);
			VLINE(samplerate);

			read_u32le(fp); // byte rate, ignored
			read_u16le(fp); // block align, ignored
			bps         = read_u16le(fp);

			VLINE(bps);
			// only support 1/2-channel 16-bit samples
			if (audioformat != 1 || bps != 16 || (numchannels != 1 && numchannels != 2)){
				fp.close();
				return NULL;
			}

			// skip ahead of the rest of the fmt chunk
			if (chunksize > 16)
				fp.seek(chunksize - 16, SeekMode::SeekCur);
		}
		
		else if (chunkid == 0x61746164)
			{ // 'data'

			// confirm we've already processed the fmt chunk
			// confirm chunk size is evenly divisible by bytes per sample
			if (!found_fmt || (chunksize % (numchannels * bps / 8)) != 0){
				fp.close();
				V2LINE(chunkid, 0x61746164);
				return NULL;
			}

			// calculate the number of samples based on the chunk size and allocate the space
			int scount = chunksize / (numchannels * bps / 8);

			VLINE(scount);
			
			sf_snd sndBufferFloat = sf_snd_new(scount, samplerate, false);

			if (sndBufferFloat == NULL){
				fp.close();
				VLINE(sndBufferFloat);
				return NULL;
			}

			// read the data and convert to stereo floating point
			int16_t L, R;

			for (int i = 0; i < scount; i++){
				// read the sample
				L = (int16_t)read_u16le(fp);
				if (numchannels == 1)
					R = L; // expand to stereo
				else
					R = (int16_t)read_u16le(fp);

				// convert the sample to floating point
				// notice that int16 samples range from -32768 to 32767, therefore we have a
				// different divisor depending on whether the value is negative or not
				if (L < 0)
					sndBufferFloat->samples[i].L = (float)L / 32768.0f;
				else
					sndBufferFloat->samples[i].L = (float)L / 32767.0f;
				if (R < 0)
					sndBufferFloat->samples[i].R = (float)R / 32768.0f;
				else
					sndBufferFloat->samples[i].R = (float)R / 32767.0f;
			}

			// we've loaded the wav data, so just return now
			VLINE(sndBufferFloat);
			fp.close();
			return sndBufferFloat;
			
		}
		else{ // skip an unknown chunk
			if (chunksize > 0)
				fp.seek(chunksize, SeekMode::SeekCur);
		}
	}

	// didn't find data chunk, so fail
	LINE;
	fp.close();
	return NULL;
}

static float clampf(float v, float min, float max){
	return v < min ? min : (v > max ? max : v);
}

// save a WAV file (returns false for error)
bool sf_wavsave(sf_snd snd, const char *file){
	File fp = SD.open(file, FILE_WRITE);
	if (fp == NULL)
		return false;

	// calculate the different file sizes based on sample size
	uint32_t size2 = snd->size * 4; // total bytes of data
	uint32_t sizeall = size2 + 36; // total file size minus 8
	if (snd->size > size2 || snd->size > sizeall || size2 > sizeall)
		return false; // sample too large

	write_u32le(fp, 0x46464952);    // 'RIFF'
	write_u32le(fp, sizeall);       // rest of file size
	write_u32le(fp, 0x45564157);    // 'WAVE'
	write_u32le(fp, 0x20746D66);    // 'fmt '
	write_u32le(fp, 16);            // size of fmt chunk
	write_u16le(fp, 1);             // audio format
	write_u16le(fp, 2);             // stereo
	write_u32le(fp, snd->rate);     // sample rate
	write_u32le(fp, snd->rate * 4); // bytes per second
	write_u16le(fp, 4);             // block align
	write_u16le(fp, 16);            // bits per sample
	write_u32le(fp, 0x61746164);    // 'data'
	write_u32le(fp, size2);         // size of data chunk

	// convert the sample to stereo 16-bit, and write to file
	for (int i = 0; i < snd->size; i++){
		float L = clampf(snd->samples[i].L, -1, 1);
		float R = clampf(snd->samples[i].R, -1, 1);
		int16_t Lv, Rv;
		// once again, int16 samples range from -32768 to 32767, so we need to scale the floating
		// point sample by a different factor depending on whether it's negative
		if (L < 0)
			Lv = (int16_t)(L * 32768.0f);
		else
			Lv = (int16_t)(L * 32767.0f);
		if (R < 0)
			Rv = (int16_t)(R * 32768.0f);
		else
			Rv = (int16_t)(R * 32767.0f);
		write_u16le(fp, (uint16_t)Lv);
		write_u16le(fp, (uint16_t)Rv);
	}

	fp.close();
	return true;
}


// load a WAV file (returns NULL for error)
sf_snd sf_randomload(const char *notused)
{
	LINE;

	// start reading chunks
	bool found_fmt = false;
	uint16_t audioformat;
	uint16_t numchannels;
	uint32_t samplerate;
	uint16_t bps;

	uint32_t chunksize = 39072;
	VLINE(chunksize);
	
	 // 'fmt '
	found_fmt = true;

	// load the fmt information
	audioformat = 1;
	VLINE(audioformat);
	
	numchannels = 1;
	VLINE(numchannels);
	
	samplerate  = 44100;
	VLINE(samplerate);

	bps         = 16;
	VLINE(bps);
	
	// only support 1/2-channel 16-bit samples
	if (audioformat != 1 || bps != 16 || (numchannels != 1 && numchannels != 2))
	{
		abort();
		return NULL;
	}
	
	// confirm we've already processed the fmt chunk
	// confirm chunk size is evenly divisible by bytes per sample
	if (!found_fmt || (chunksize % (numchannels * bps / 8)) != 0)
	{
		abort();
		return NULL;
	}

	// calculate the number of samples based on the chunk size and allocate the space
	int scount = chunksize / (numchannels * bps / 8);
	VLINE(scount);

	// allocate the buffer	
	sf_snd sndBufferFloat = sf_snd_new(scount, samplerate, false);

	if (sndBufferFloat == NULL){
		VLINE(sndBufferFloat);
		abort();
		return NULL;
	}

	// read the data and convert to stereo floating point
	int16_t L, R;
	
	randomSeed(millis());

	for (int i = 0; i < scount; i++)
	{
		// read the sample
		L = (int16_t) random(-30000, 30000);
		
		if (numchannels == 1)
			R = L; // expand to stereo
		else
			R = random(-30000, 30000);

		// convert the sample to floating point
		// notice that int16 samples range from -32768 to 32767, therefore we have a
		// different divisor depending on whether the value is negative or not
		
		if (L < 0)
			sndBufferFloat->samples[i].L = (float)L / 32768.0f;
		else
			sndBufferFloat->samples[i].L = (float)L / 32767.0f;
		if (R < 0)
			sndBufferFloat->samples[i].R = (float)R / 32768.0f;
		else
			sndBufferFloat->samples[i].R = (float)R / 32767.0f;
	}

	// we've loaded the wav data, so just return now
	VLINE(sndBufferFloat);
	return sndBufferFloat;
	
}

