/**
 * some code from pico-playground
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * some code from Ben Everard
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define PICO_AUDIO_I2S_DATA_PIN 9
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 10

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "bongo.h"
#include "picopicosynth.h"


#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif

//not too sure how to deal with this as a library?
//does it need to be compiled separately and have different versions for each?
//probably need to have a file that can be included differently with the init_audio function in it.
#if USE_AUDIO_I2S
#include "pico/audio_i2s.h"
#elif USE_AUDIO_PWM
#include "pico/audio_pwm.h"
#elif USE_AUDIO_SPDIF
#include "pico/audio_spdif.h"
#endif

#define SINE_WAVE_TABLE_LEN0 2048
#define SINE_WAVE_TABLE_LEN1 1024


#define SAMPLES_PER_BUFFER 256

#define BEATFREQ 15000
#define BEATNUM 8

//not sure this is being used correctly?
#define SAMPLEFREQ 12050

//taken from the bongosamples header file -- should be stored in there really
#define BONGOSAMPLES 9054


static int16_t bongo_wave_table[BONGOSAMPLES];


static int16_t sine_wave_table_0[SINE_WAVE_TABLE_LEN0];
static int16_t sine_wave_table_1[SINE_WAVE_TABLE_LEN1];



int16_t bongos(int posn, int note) {
	
	if (note >= 0 ) {
		return no_envelope(bongo_wave_table, BONGOSAMPLES, 1, posn);
	}
	else {
		return 0;
	}
}

int16_t low_sine(int posn, int note) {
	if (note == 0 ) {
		return bitcrush(envelope(sine_wave_table_0, SINE_WAVE_TABLE_LEN0, 24, posn, posn, 5000, 10000, 15000, 40000),32768,8);
	}
	if (note == 1 ) {
		return bitcrush(envelope(sine_wave_table_1, SINE_WAVE_TABLE_LEN1, 24, posn, posn, 5000, 10000, 15000, 40000),32768,8);
	}
	else {
		return 0;
	}
}

int main() {
	
	stdio_init_all();


    for (int i = 0; i < SINE_WAVE_TABLE_LEN0; i++) {
        sine_wave_table_0[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN0));
    }
	
    for (int i = 0; i < SINE_WAVE_TABLE_LEN1; i++) {
        sine_wave_table_1[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN1));
    }
	
	for (int i = 0; i < BONGOSAMPLES; i++) {
		bongo_wave_table[i] = bongoSamples[i] * 256;
		//the sound sample web page is +/-1 128 so need to multiply this up :https://bitluni.net/wp-content/uploads/2018/03/WavetableEditor.html
	}	

	//MUST INCLUDE THIS LINE TO SET UP AUDIO
	//CURRENTLY I2S IS THE ONLY OPTION
    struct audio_buffer_pool *ap = init_audio_i2s(SAMPLES_PER_BUFFER, PICO_AUDIO_I2S_DATA_PIN, PICO_AUDIO_I2S_CLOCK_PIN_BASE);

	//note -- these must be exactly the right length
	//our could these be numbers? 0 being not play, 1 or more being playing?
	//less than 0 is not playing, > 0 is playing
	int bongo_sequence[] = {1, -1, 1, -1, -1, -1, -1, -1};
	int low_sine_sequence[] = {1, -1, -1, 0, -1, -1, -1, -1};
	
	struct sequencer main_sequencer;
	init_sequencer(&main_sequencer, BEATNUM, BEATFREQ);
	
	//add up to 32 different sequences here
	add_sequence(&main_sequencer, 0, bongo_sequence, bongos, 0.8);
	add_sequence(&main_sequencer, 1, low_sine_sequence, low_sine, 0.8);
	
    while (true) {
		
		//do any processing you want here
		
		//MUST INCLUDE THIS LINE TO PASS THE CURRENT BUFFER TO THE AUDIO SYSTEM
        give_audio_buffer(ap, fill_next_buffer(&main_sequencer, ap, SAMPLES_PER_BUFFER));
    }
    return 0;
}