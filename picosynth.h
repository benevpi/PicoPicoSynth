
//Things that manipulate sound
int16_t no_envelope(int16_t wave_table[], int table_len, float table_multiplier, int envelope_posn);
int16_t envelope(int16_t wave_table[], int table_len, float table_multiplier,float posn_virtual, int envelope_posn, int decay, int sustain, int release, int finish);
int16_t mixer(int16_t inputs[], float volumes[], int size);
int sequence( bool note_sequence[], int posn_absolute, int current_beat_num, int last_loop_posn);

//some basic oscillators (note the float output as these are designed as multipliers
float square_wobbler(int wave_len,float vol, int posn_absolute);
float saw_wobbler(int wave_len,float vol, int posn_absolute);
float triangle_wobbler(int wave_len,float vol, int posn_absolute);

//Effects that can be applied to sounds
int16_t bitcrush(int16_t input, int max, int new_max);
int16_t create_echo(int16_t input, int16_t buffer[], int buffer_len, int posn_absolute);
int16_t read_echo(int16_t buffer[], int buffer_len, int posn_absolute, int delay);