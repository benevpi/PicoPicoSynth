cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(speedramp)

pico_sdk_init()

# only build PWM example if library is available
if (TARGET pico_audio_pwm)
    add_executable(picopicosynth_pwm
        picopicosynth.c
    )

    target_link_libraries(picopicosynth_pwm PRIVATE
        pico_stdlib
        pico_audio_pwm
    )

    target_compile_definitions(picopicosynth_pwm PRIVATE
        #define for our example code
        USE_AUDIO_PWM=1
    )
    # create map/bin/hex file etc.
    pico_add_extra_outputs(picopicosynth_pwm)
endif()
