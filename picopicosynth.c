/**
 * Copyright (c) 2021 Ben Everard
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "picopicosynth.h"
#include "pico/audio_i2s.h"

#include <stdio.h>
#include <math.h>



int16_t no_envelope(struct wavetable *this_wavetable, float table_multiplier, int envelope_posn) {
	if((envelope_posn*table_multiplier) >= this_wavetable->length) { return 0; }
	
	return this_wavetable->samples[(int)(envelope_posn*table_multiplier)] ;
}

//note posn_virtual only works out-the-box with continuous waves.
// can probably be made to work with samples, but will need a little thought.
int16_t envelope(struct wavetable *this_wavetable, float table_multiplier,float posn_virtual, int envelope_posn, int decay, int sustain, int release, int finish) {
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
	return this_wavetable->samples[(int)((float)posn_virtual*table_multiplier) % this_wavetable->length ] * proportion;
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
int sequence( int note_sequence[], int posn_absolute, int current_beat_num, int last_loop_posn, int beat_num, int beat_freq) {
	// returns the length of time (in samples) since last played
	int last_played = 0;
	for( int i=0; i<beat_num;i++) {
		if ((note_sequence[i] >= 0) && i<= current_beat_num) {
			last_played = last_loop_posn + (i*beat_freq);
		}
	}
	
	//wrap around to previous bar if none in current bar
	if(last_played == 0) {
		for(int i=0; i<beat_num;i++) {
			if (note_sequence[i] >= 0) {
				last_played = last_loop_posn + (i*beat_freq) - (beat_freq*beat_num);
			}
		}	
	}
	return posn_absolute - last_played;
}


//a set of LFOs of basic waveforms. May be worth expanding these with a wave table.
//Don't think it's currently possible to use these.
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


//Let's do the more structural atuff here to see how it works

void init_sequencer(struct sequencer *this_sequencer, int beats, int beat_freq){
	this_sequencer->beat_num = 0;
	this_sequencer->max_beat_num = beats;
	this_sequencer->beat_freq = beat_freq;
	
	this_sequencer->posn_absolute = 0;
	this_sequencer->last_loop_posn = 0;
	
	for(int i=0;i<32;i++) {
		this_sequencer->mute_inputs[i] = true;
		this_sequencer->volumes[i] = 0;
	}
}

int16_t update_sequencer(struct sequencer *this_sequencer ){
	float output_volume_f = 0;
	
	//let's update the positions and beat numbers
	if (this_sequencer->posn_absolute > (this_sequencer->last_loop_posn + ((this_sequencer->beat_num+1) * this_sequencer->beat_freq))) { this_sequencer->beat_num++; }
		if ( this_sequencer->beat_num == this_sequencer->max_beat_num ) {
			this_sequencer->beat_num = 0; 
			this_sequencer->last_loop_posn = this_sequencer->posn_absolute;
			}
	
	for(int i=0; i<32; i++) {
		if(this_sequencer->mute_inputs[i] == false) {
			
			//need the last played note
			//this feels a bit ugly. Might be a better way of doing this.
			int last_note = -1;
			for( int j=0; j<this_sequencer->max_beat_num;j++) {
				if ((this_sequencer->sequences[i].beats[j]>=0) && j<= this_sequencer->beat_num) {
					last_note = this_sequencer->sequences[i].beats[j];
				}
			}
	
			//wrap around to previous bar if none in current bar
			if(last_note < 0) {
				for(int j=0; j<this_sequencer->max_beat_num;j++) {
					if (this_sequencer->sequences[i].beats[j] >=0) {
						last_note = this_sequencer->sequences[i].beats[j];
					}
				}	
			}
			
			
			//last_note = 0;
			
			output_volume_f = output_volume_f + (this_sequencer->volumes[i] * (float)this_sequencer->sequences[i].source(sequence(
				this_sequencer->sequences[i].beats, 
				this_sequencer->posn_absolute,
				this_sequencer->beat_num,
				this_sequencer->last_loop_posn,
				this_sequencer->max_beat_num,
				this_sequencer->beat_freq),
				last_note));	
		}
	}
	
	//ugly hack. remove! will cause a stutter after circa 2 1/2 hours
	this_sequencer->posn_absolute++;
	if (this_sequencer->posn_absolute > 2000000000) { this_sequencer->posn_absolute = 0;}
	
	//this should catch overflow here!!
	return (int16_t)output_volume_f;
			
}

void add_sequence(struct sequencer *this_sequencer, int number, int beats[], int16_t (*source)(int posn, int note), float volume) {
	for(int i=0;i<this_sequencer->max_beat_num;i++) {
		this_sequencer->sequences[number].beats[i] = beats[i];
	}
	this_sequencer->sequences[number].source = source;
	this_sequencer->volumes[number] = volume;
	this_sequencer->mute_inputs[number] = false;
}

struct audio_buffer * fill_next_buffer(struct sequencer *this_sequencer, struct audio_buffer_pool *ap, int num_samples) {
	struct audio_buffer *buffer = take_audio_buffer(ap, true); // is this the point the loop blocks waiting for free space?
	int16_t *samples = (int16_t *) buffer->buffer->bytes;
	
	
	for (uint i = 0; i < num_samples; i++) {
		samples[i] = update_sequencer(this_sequencer);
	}
	
	buffer->sample_count = num_samples;
	return buffer;
}


struct audio_buffer_pool *init_audio_i2s(int buffer_size, int data_pin, int clock_pin) {

    static audio_format_t audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .sample_freq = 24000,
            .channel_count = 1,
    };
	

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      buffer_size); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;

	struct audio_i2s_config config = {
			.data_pin = data_pin,
			.clock_pin_base = clock_pin,
			.dma_channel = 0,
			.pio_sm = 0,
	};

	output_format = audio_i2s_setup(&audio_format, &config);
	if (!output_format) {
		panic("PicoAudio: Unable to open audio device.\n");
	}

	ok = audio_i2s_connect(producer_pool);
	assert(ok);
	audio_i2s_set_enabled(true);

    return producer_pool;
}



struct wavetable * get_sinewave_table(float wave_frequency, float system_frequency) {
	int length = system_frequency / wave_frequency;
	struct wavetable *this_wavetable = malloc( sizeof(struct wavetable) + sizeof(int16_t)*length);
	
	this_wavetable->length = length;
	
	for (int i = 0; i < length; i++) {
        this_wavetable->samples[i] = 32767 * cosf(i * 2 * (float) (M_PI / length));
    }
	
	return this_wavetable;
}

struct wavetable * create_wavetable(int length) {
	struct wavetable *this_wavetable = malloc( sizeof(struct wavetable) + sizeof(int16_t)*length);
	this_wavetable->length = length;	
	return this_wavetable;
}
