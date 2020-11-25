/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <math.h>
#include "bongo.h"

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif

#include "pico/stdlib.h"


#include "pico/audio_pwm.h"

//note -- will have to include this in the cmake file
#include "hardware/gpio.h"

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

//There is definately a better way of storing all this
//actually, this shouldn't be needed any more
#define HIGHBEATFREQ 1.2
#define LOWBEATFREQ 4.8

#define BEATFREQ 15000
#define BEATNUM 8

//not sure this is being used correctly?
#define SAMPLEFREQ 12050

//taken from the bongosamples header file -- should be stored in there really
#define BONGOSAMPLES 9054

#define BUTTON_GPIO 1

//this gives a max echo of 1 second
#define ECHOBUFFERLEN 22050

/**TODO
# weird blip if there's echo on bongos and sine all at the same time. Not sure if running out of resources or something else.
# would like to try with I2S maybe.
#this may be fixed (ish) in the audio correction mode
#should still try I2S

# Defo needs flashing lights. Hook up some neoPixels and maybe buttons to control the sequencer


##getting some clicks and hisses -- could this be noise on the ground when doing a lot of processing?
#fix saw / square wobbler blinps
# add some effects
## echo (could also be used for chorus)
### needs a circular buffer
### or could just be a regular buffer that's kicked off on envelope & noenvelope
### or could just hold a separate posn_absolute (posn_echo) and echo_len and run some more inputs to the mixers. -- this won't work as it'll mess with the beat numbers, etc.
# Also, can I pull the functions out into a separate file so they can be used elsewhere?

# how to make it physical computing?
## neopixels that light up the beat number?
## take user input? Get a irq on a GPIO and use that to trigger certain sounds
### Done, but should it there be a bit more debouncing than a simple falling edge?
## use a potentiometer to control pitch / frequency?

**/


static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];
static int16_t bongo_wave_table[BONGOSAMPLES];
static int16_t echo_table[ECHOBUFFERLEN];
static int16_t calc_samples[SAMPLES_PER_BUFFER];

uint32_t button_last_pressed = 0;
uint32_t posn_absolute = 0;

//currently documentation incomplete on this. Need to have a read over what various settings do when it's finished.
struct audio_buffer_pool *init_audio() {

    static struct audio_format audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .sample_freq = 22050,
            .channel_count = 1,
    };


    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 2
    };

	//this says there should be three buffers.
    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;

    output_format = pio_pwm_audio_setup(&audio_format, -1, &default_mono_channel_config); //-1 is don't care about latency
    if (!output_format) {
        panic("MuAudio: Unable to open audio device.\n");
    }
    ok = pio_pwm_audio_default_connect(producer_pool, false); // the true dedicates core 1 to doing audio gubbins -- doesn't seem to work?. maybe need to enable multicore? --looking at source, it's not implemented yet
    assert(ok);
    pio_pwm_audio_enable(true);

    return producer_pool;
}

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
int sequence( bool note_sequence[], int posn_absolute, int current_beat_num, int last_loop_posn) {
	// returns the length of time (in samples) since last played
	int last_played = 0;
	for( int i=0; i<BEATNUM;i++) {
		if (note_sequence[i] && i<= current_beat_num) {
			last_played = last_loop_posn + (i*BEATFREQ);
		}
	}
	
	//wrap around to previous bar if none in current bar
	if(last_played == 0) {
		for(int i=0; i<BEATNUM;i++) {
			if (note_sequence[i]) {
				last_played = last_loop_posn + (i*BEATFREQ) - (BEATFREQ*BEATNUM);
			}
		}	
	}
	return posn_absolute - last_played;
}

void button_callback() {
	button_last_pressed = posn_absolute;	
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


int main() {
	//do we really need this?
	//does seem to work a little better
	set_sys_clock_48();
	//setup our button
	gpio_init(BUTTON_GPIO);
    gpio_dir(BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(BUTTON_GPIO);
	
	//should there be any sort of debounce here?
	gpio_irq_enable_with_callback(BUTTON_GPIO, GPIO_IRQ_EDGE_FALL, true, &button_callback);



    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        sine_wave_table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }
	
	for (int i = 0; i < BONGOSAMPLES; i++) {
		bongo_wave_table[i] = bongoSamples[i] * 256;
		//the sound sample web page is +/-1 128 so need to multiply this up :https://bitluni.net/wp-content/uploads/2018/03/WavetableEditor.html
	}	

    struct audio_buffer_pool *ap = init_audio();

	
	int beat_num = 0;
	float posn_low_sine = 0;
	int last_loop_posn = 0;
	bool bongo_sequence[] = {true, false, true, false, false, false, false, false};
	bool low_sine_sequence[] = {true, false, false, true, false, false, false, false};
	
	int16_t inputs[] = {0,0,0,0};
	float volumes[] = {0.4,0.1,0.1,0.2};
	int mixer_size = 4;
	
	int posn_echo = 0;
	
	enum audio_correction_mode m = pio_pwm_audio_get_correction_mode();
	m = fixed_dither;
	pio_pwm_audio_set_correction_mode(m);
	
    while (true) {
		
		//there are three buffers, so presumably, this will get the next one rather than just using the same one over and over?
		//check this with Graham?
        struct audio_buffer *buffer = take_audio_buffer(ap, true); // is this the point the loop blocks waiting for free space?
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        
		
		for (uint i = 0; i < SAMPLES_PER_BUFFER; i++) {
			
			//if(posn_absolute > ECHOLEN) { posn_echo = posn_absolute - echo_len; }
			
			if (posn_absolute > (last_loop_posn + ((beat_num+1) * BEATFREQ))) { beat_num++; }
			if ( beat_num == BEATNUM ) {
				beat_num = 0; 
				last_loop_posn = posn_absolute;
				}
				
			//track a 'virtual' position for the low_sine note. This allows us to use posn_absolute for beats & enveloping, but the virtual position for
			//wobbles additionally to give a tremello effect.
			//the alternative to this, I think, would be to feed the output into a buffer large enough to apply the tremello effect in there
			posn_low_sine = posn_low_sine + 1 + triangle_wobbler(2000,0.5,posn_absolute);
			
			//bongos
			inputs[0] = create_echo(no_envelope(bongo_wave_table, BONGOSAMPLES, 1, sequence(bongo_sequence, posn_absolute, beat_num, last_loop_posn))
									, echo_table, ECHOBUFFERLEN, posn_absolute);
			
			// weird distorted sine wave
			inputs[1] = bitcrush(envelope(sine_wave_table, SINE_WAVE_TABLE_LEN, 24, posn_low_sine, sequence(low_sine_sequence, posn_absolute, beat_num, last_loop_posn), 
												5000, 10000, 15000, 40000),32768,8);
			//undistorted sound wave
			//inputs[1] = envelope(sine_wave_table, SINE_WAVE_TABLE_LEN, 8, posn_low_sine, sequence(low_sine_sequence, posn_absolute, beat_num, last_loop_posn), 
			//									5000, 10000, 15000, 40000);
			
												
			// high pitched sine on button press
			/**
			inputs[2] = envelope(sine_wave_table, SINE_WAVE_TABLE_LEN, 32, posn_absolute, posn_absolute-button_last_pressed, 
									     5000, 10000, 15000, 20000);
			**/
			
			//echo of bongos
			inputs[3] = read_echo(echo_table, ECHOBUFFERLEN, posn_absolute, 2000);
			
			
			samples[i] = mixer (inputs, volumes, mixer_size);
			posn_absolute++;
			if (posn_absolute > 2000000000) { posn_absolute = 0;} // ugly hack. Should do something better --this will also break the sequencer -- only causes a problem after 2 1/2 hours, so could just leave it for now.
        }



		
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    return 0;
}
