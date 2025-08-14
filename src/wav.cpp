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

	fp = SD.open(file, FILE_WRITE);
	//FILE *fp = fopen(file, "rb");
	
	if (fp == NULL)
	{
		Serial.printf("cannot open for reading %s\n", file);
		return NULL;
	}

	uint32_t riff = read_u32le(fp);
	if (riff != 0x46464952){ // 'RIFF'
		fp.close();
		return NULL;
	}

	read_u32le(fp); // filesize; don't really care about this

	uint32_t wave = read_u32le(fp);
	if (wave != 0x45564157){ // 'WAVE'
		fp.close();
		return NULL;
	}

	// start reading chunks
	bool found_fmt = false;
	uint16_t audioformat;
	uint16_t numchannels;
	uint32_t samplerate;
	uint16_t bps;
	while (!fp.available()){
		uint32_t chunkid = read_u32le(fp);
		uint32_t chunksize = read_u32le(fp);
		if (chunkid == 0x20746D66){ // 'fmt '

			// confirm we haven't already processed the fmt chunk, and that it's a good size
			if (found_fmt || chunksize < 16){
				fp.close();
				return NULL;
			}

			found_fmt = true;

			// load the fmt information
			audioformat = read_u16le(fp);
			numchannels = read_u16le(fp);
			samplerate  = read_u32le(fp);
			read_u32le(fp); // byte rate, ignored
			read_u16le(fp); // block align, ignored
			bps         = read_u16le(fp);

			// only support 1/2-channel 16-bit samples
			if (audioformat != 1 || bps != 16 || (numchannels != 1 && numchannels != 2)){
				fp.close();
				return NULL;
			}

			// skip ahead of the rest of the fmt chunk
			if (chunksize > 16)
				fp.seek(chunksize - 16, SeekMode::SeekCur);
		}
		else if (chunkid == 0x61746164){ // 'data'

			// confirm we've already processed the fmt chunk
			// confirm chunk size is evenly divisible by bytes per sample
			if (!found_fmt || (chunksize % (numchannels * bps / 8)) != 0){
				fp.close();
				return NULL;
			}

			// calculate the number of samples based on the chunk size and allocate the space
			int scount = chunksize / (numchannels * bps / 8);
			sf_snd snd = sf_snd_new(scount, samplerate, false);
			if (snd == NULL){
				fp.close();
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
					snd->samples[i].L = (float)L / 32768.0f;
				else
					snd->samples[i].L = (float)L / 32767.0f;
				if (R < 0)
					snd->samples[i].R = (float)R / 32768.0f;
				else
					snd->samples[i].R = (float)R / 32767.0f;
			}

			// we've loaded the wav data, so just return now
			fp.close();
			return snd;
		}
		else{ // skip an unknown chunk
			if (chunksize > 0)
				fp.seek(chunksize, SeekMode::SeekCur);
		}
	}

	// didn't find data chunk, so fail
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
