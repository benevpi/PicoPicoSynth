cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(picopicosynth_example1)

pico_sdk_init()

ADD_LIBRARY(PicoPicoSynth picopicosynth.c)
target_link_libraries(PicoPicoSynth pico_stdlib pico_audio_i2s)

add_executable(picopicosynth_example1
	example_synth.c
)

# enable usb output, disable uart output
    pico_enable_stdio_usb(picopicosynth_example1 1)
    pico_enable_stdio_uart(picopicosynth_example1 0)

target_link_libraries(picopicosynth_example1 PRIVATE
	pico_stdlib
	pico_audio_i2s
	PicoPicoSynth
)

target_compile_definitions(picopicosynth_example1 PRIVATE
	#define for our example code
	USE_AUDIO_I2S=1
	PICO_AUDIO_I2S_MONO_INPUT=1
)
# create map/bin/hex file etc.
pico_add_extra_outputs(picopicosynth_example1)

