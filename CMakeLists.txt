cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(picopicosynth)

pico_sdk_init()

# only build PWM example if library is available
if (TARGET pico_audio_pwm)
    add_executable(picopicosynth_i2s
        picopicosynth.c
    )

    target_link_libraries(picopicosynth_i2s PRIVATE
        pico_stdlib
        pico_audio_i2s
    )

    target_compile_definitions(picopicosynth_i2s PRIVATE
        #define for our example code
		USE_AUDIO_I2S=1
		PICO_AUDIO_I2S_MONO_INPUT=1
    )
    # create map/bin/hex file etc.
    pico_add_extra_outputs(picopicosynth_i2s)
endif()
