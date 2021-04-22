#include "pico/audio_i2s.h"

#define PWM 0
#define I2S 1
#define SPDIF 2

//Things that manipulate sound
int16_t no_envelope(int16_t wave_table[], int table_len, float table_multiplier, int envelope_posn);
int16_t envelope(int16_t wave_table[], int table_len, float table_multiplier,float posn_virtual, int envelope_posn, int decay, int sustain, int release, int finish);
int16_t mixer(int16_t inputs[], float volumes[], int size);
int sequence( int note_sequence[], int posn_absolute, int current_beat_num, int last_loop_posn,int beat_num, int beat_freq);

//some basic oscillators (note the float output as these are designed as multipliers
float square_wobbler(int wave_len,float vol, int posn_absolute);
float saw_wobbler(int wave_len,float vol, int posn_absolute);
float triangle_wobbler(int wave_len,float vol, int posn_absolute);

//Effects that can be applied to sounds
int16_t bitcrush(int16_t input, int max, int new_max);
int16_t create_echo(int16_t input, int16_t buffer[], int buffer_len, int posn_absolute);
int16_t read_echo(int16_t buffer[], int buffer_len, int posn_absolute, int delay);

//setup sequencer
//sod it, no point it getting too precious with memory -- let's just have max of 32 beats and 32 inputs
// Note -- should the source pass the beat number as well to allow it to change pitch  / sound / etc on different beats?
struct sequence {
	int16_t (*source)(int posn, int note);
	int beats[32];
};

struct sequencer {
	int beat_num;
	int max_beat_num;
	int beat_freq;
	
	int posn_absolute;
	int last_loop_posn;
	
	float volumes[32];
	bool mute_inputs[32];
	struct sequence sequences[32];
};

struct sequencer create_new_sequencer();
void init_sequencer(struct sequencer *this_sequencer, int beats, int beat_freq);
int16_t update_sequencer();
void update_sequence();
void add_sequence(struct sequencer *this_sequencer, int number, int beats[], int16_t (*source)(int posn, int note), float volume);

struct audio_buffer * fill_next_buffer(struct sequencer *this_sequencer, struct audio_buffer_pool *ap, int num_samples);

struct audio_buffer_pool *init_audio_i2s(int buffer_size, int data_pin, int clock_pin);




