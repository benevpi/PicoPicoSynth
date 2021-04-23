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

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"


//TODO
//Let's have a per-bar callback that allows us to evolve the sound as time goes on. Possibly with response to external stimulus.

//let's just stick with I2S for now.
#include "pico/audio_i2s.h"

#define SAMPLES_PER_BUFFER 256

#define BEATFREQ 10000
#define BEATNUM 8

//taken from the bongosamples header file -- should be stored in there really
#define BONGOSAMPLES 9054


static int16_t bongo_wave_table[BONGOSAMPLES];

struct wavetable *low_sine_0;
struct wavetable *low_sine_1;
struct wavetable *bongo_table;


//callback functions that are run every sample to get the correct data for that point in time.
//note these sequencer functions don't have to return sound, but they can if they want to.
int16_t bongos(int posn, int note) {
	
	if (note == 0 ) {
		return no_envelope(bongo_table, 1, posn);
	}
	if (note == 1 ) {
		return no_envelope(bongo_table, 0.5, posn);
	}
	else {
		return 0;
	}
}

int16_t low_sine(int posn, int note) {
	if (note == 0 ) {
		return bitcrush(envelope(low_sine_0, 1, posn, posn, 5000, 10000, 15000, 40000),32768,8);
	}
	if (note == 1 ) {
		return bitcrush(envelope(low_sine_1, 1, posn, posn, 5000, 10000, 15000, 40000),32768,8);
	}
	else {
		return 0;
	}
}

int main() {
	
	stdio_init_all();
	
	low_sine_0 = get_sinewave_table(50, 24000);
	low_sine_1 = get_sinewave_table(100, 24000);
	bongo_table = create_wavetable(9054);
	
	for (int i = 0; i < BONGOSAMPLES; i++) {
		bongo_table->samples[i] = bongoSamples[i] * 256;
		//the sound sample web page is +/-1 128 so need to multiply this up :https://bitluni.net/wp-content/uploads/2018/03/WavetableEditor.html
	}	

	//MUST INCLUDE THIS LINE TO SET UP AUDIO
	//CURRENTLY I2S IS THE ONLY OPTION
    struct audio_buffer_pool *ap = init_audio_i2s(SAMPLES_PER_BUFFER, PICO_AUDIO_I2S_DATA_PIN, PICO_AUDIO_I2S_CLOCK_PIN_BASE);

	//note -- these must be exactly the right length
	//our could these be numbers? 0 being not play, 1 or more being playing?
	//less than 0 is not playing, > 0 is playing
	int bongo_sequence[] = {0, 0, -1, -1, -1, 1, -1, -1};
	int low_sine_sequence[] = {-1, -1, 1, -1, -1, -1, 0, -1};
	
	struct sequencer main_sequencer;
	init_sequencer(&main_sequencer, BEATNUM, BEATFREQ);
	
	//add up to 32 different sequences here
	add_sequence(&main_sequencer, 0, bongo_sequence, bongos, 0.5);
	add_sequence(&main_sequencer, 1, low_sine_sequence, low_sine, 0.2);
	
    while (true) {
		
		//do any processing you want here
		
		//MUST INCLUDE THIS LINE TO PASS THE CURRENT BUFFER TO THE AUDIO SYSTEM
        give_audio_buffer(ap, fill_next_buffer(&main_sequencer, ap, SAMPLES_PER_BUFFER));
    }
    return 0;
}