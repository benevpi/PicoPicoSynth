/**
 * Copyright (c) 2021 Ben Everard
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "picopicosynth.h"



int16_t no_envelope(int16_t wave_table[], int table_len, float table_multiplier, int envelope_posn) {
	if((envelope_posn*table_multiplier) >= table_len) { return 0; }
	
	return wave_table[(int)(envelope_posn*table_multiplier)] ;
}

//note posn_virtual only works out-the-box with continuous waves.
// can probably be made to work with samples, but will need a little thought.
int16_t envelope(int16_t wave_table[], int table_len, float table_multiplier,float posn_virtual, int envelope_posn, int decay, int sustain, int release, int finish) {
	float attack_multiplier = 1;
	float sustain_multiplier = 0.5;
	float proportion = 0;
	//note finished
	if(envelope_posn > finish) {return 0;}
	//release
	else if(envelope_posn > release) {
		proportion = (1.0-(((float)envelope_posn - release) / ((float)finish - release))) * sustain_multiplier;
	}
	//sustain
	else if(envelope_posn > sustain) {
		proportion = sustain_multiplier;
	}
	//decay
	else if(envelope_posn > decay) {
		proportion = ( (1.0-(((float)envelope_posn - decay) / ((float)sustain - decay))) * ((float)attack_multiplier - sustain_multiplier)) + sustain_multiplier;
	}
	//attack
	else {
		proportion = ((float)envelope_posn / decay) * attack_multiplier;
	}
	return wave_table[(int)((float)posn_virtual*table_multiplier) % table_len ] * proportion;
}

int16_t mixer(int16_t inputs[], float volumes[], int size) {
	float output = 0;
	
	for(int i=0;i<size; i++) {
		output = output + inputs[i] * volumes[i];
	}
	
	//clip if output too high
	if (output > 32766) { output = 32766; }
	if (output < -32766) { output = -32766; }
	
	
	return (int16_t)output;
		
}

//wildly overkill to calculate this every sample. This only needs to be calculated every beat
int sequence( bool note_sequence[], int posn_absolute, int current_beat_num, int last_loop_posn, int beat_num, int beat_freq) {
	// returns the length of time (in samples) since last played
	int last_played = 0;
	for( int i=0; i<beat_num;i++) {
		if (note_sequence[i] && i<= current_beat_num) {
			last_played = last_loop_posn + (i*beat_freq);
		}
	}
	
	//wrap around to previous bar if none in current bar
	if(last_played == 0) {
		for(int i=0; i<beat_num;i++) {
			if (note_sequence[i]) {
				last_played = last_loop_posn + (i*beat_freq) - (beat_freq*beat_num);
			}
		}	
	}
	return posn_absolute - last_played;
}


//a set of LFOs of basic waveforms. May be worth expanding these with a wave table.
float square_wobbler(int wave_len,float vol, int posn_absolute){
	return ((float)((int)(posn_absolute/wave_len) % 2) * vol);
	
}

float saw_wobbler(int wave_len,float vol, int posn_absolute){
	return (((float)((posn_absolute) % wave_len)/wave_len) * vol);
	
}

float triangle_wobbler(int wave_len,float vol, int posn_absolute) {
	if (posn_absolute % (2*wave_len) < wave_len / 2) {
	 return (((float)((posn_absolute) % wave_len)/wave_len) * vol);
	}
	else {
		( (1.0-((float)((posn_absolute) % wave_len)/wave_len)) * vol);
	}
	
}
//very basic bitcrusher
//only resolution reduction, not sample rate reduction
//max is the number of sample levels you want, so not tied to integer bit rates
int16_t bitcrush(int16_t input, int max, int new_max) {
	float reducer = max / new_max;
	
	return ((int)(input / reducer)) * reducer;
}

int16_t create_echo(int16_t input, int16_t buffer[], int buffer_len, int posn_absolute) {
	buffer[posn_absolute % buffer_len] = input;
	return input;
}

int16_t read_echo(int16_t buffer[], int buffer_len, int posn_absolute, int delay) {
	if (posn_absolute < delay) { return 0;}
	return buffer[(posn_absolute - delay) % buffer_len];
	
}