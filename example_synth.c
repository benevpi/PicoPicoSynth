/**
 * some code from pico-playground
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * some code from Ben Everard
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "bongo.h"
#include "picopicosynth.h"

#define PICO_AUDIO_I2S_DATA_PIN 9
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 10

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif



#if USE_AUDIO_I2S

#include "pico/audio_i2s.h"

#elif USE_AUDIO_PWM
#include "pico/audio_pwm.h"
#elif USE_AUDIO_SPDIF
#include "pico/audio_spdif.h"
#endif

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

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

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];
static int16_t bongo_wave_table[BONGOSAMPLES];
static int16_t echo_table[ECHOBUFFERLEN];
static int16_t calc_samples[SAMPLES_PER_BUFFER];

uint32_t button_last_pressed = 0;
uint32_t posn_absolute = 0;

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];

struct audio_buffer_pool *init_audio() {

    static audio_format_t audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
#if USE_AUDIO_SPDIF
            .sample_freq = 44100,
#else
            .sample_freq = 24000,
#endif
            .channel_count = 1,
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;
#if USE_AUDIO_I2S
    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
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
#elif USE_AUDIO_PWM
    output_format = audio_pwm_setup(&audio_format, -1, &default_mono_channel_config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }
    ok = audio_pwm_default_connect(producer_pool, false);
    assert(ok);
    audio_pwm_set_enabled(true);
#elif USE_AUDIO_SPDIF
    output_format = audio_spdif_setup(&audio_format, &audio_spdif_default_config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }
    //ok = audio_spdif_connect(producer_pool);
    ok = audio_spdif_connect(producer_pool);
    assert(ok);
    audio_spdif_set_enabled(true);
#endif
    return producer_pool;
}

void button_callback() {
	button_last_pressed = posn_absolute;	
}

int main() {
	//do we really need this?
	//does seem to work a little better
	//set_sys_clock_48();
	//setup our button
	gpio_init(BUTTON_GPIO);
    gpio_set_dir(BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(BUTTON_GPIO);
	
	//should there be any sort of debounce here?
	gpio_set_irq_enabled_with_callback(BUTTON_GPIO, GPIO_IRQ_EDGE_FALL, true, &button_callback);



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
			//I think this is what's going weird?nope 
			inputs[0] = create_echo(no_envelope(bongo_wave_table, BONGOSAMPLES, 1, sequence(bongo_sequence, posn_absolute, beat_num, last_loop_posn, BEATNUM, BEATFREQ))
									, echo_table, ECHOBUFFERLEN, posn_absolute);
			
			// weird distorted sine wave
			
			inputs[1] = bitcrush(envelope(sine_wave_table, SINE_WAVE_TABLE_LEN, 24, posn_low_sine, sequence(low_sine_sequence, posn_absolute, beat_num, last_loop_posn, BEATNUM, BEATFREQ), 
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
			
			
			samples[i] = mixer(inputs, volumes, mixer_size);
			
			
			//simple test
			/**
			inputs[0] = envelope(sine_wave_table, SINE_WAVE_TABLE_LEN, 8, posn_low_sine, sequence(low_sine_sequence, posn_absolute, beat_num, last_loop_posn), 
												5000, 10000, 15000, 40000);
			samples[i] = mixer(inputs, volumes, 1);
			**/								
			
			posn_absolute++;
			if (posn_absolute > 2000000000) { posn_absolute = 0;} // ugly hack. Should do something better --this will also break the sequencer -- only causes a problem after 2 1/2 hours, so could just leave it for now.
        }



		
        buffer->sample_count = SAMPLES_PER_BUFFER;
        give_audio_buffer(ap, buffer);
    }
    return 0;
}