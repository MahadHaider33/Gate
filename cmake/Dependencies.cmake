set(RN ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise)
set(SP ${CMAKE_CURRENT_SOURCE_DIR}/third_party/speexdsp)
if(NOT EXISTS "${RN}/src/rnnoise_data.c" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/q/infra/include/infra/support.hpp")
  message(STATUS "Dependencies must be prepared with scripts/bootstrap.ps1 before configuring.")
endif()
add_library(rnnoise STATIC
  ${RN}/src/denoise.c ${RN}/src/rnn.c ${RN}/src/pitch.c
  ${RN}/src/kiss_fft.c ${RN}/src/celt_lpc.c ${RN}/src/nnet.c
  ${RN}/src/nnet_default.c ${RN}/src/parse_lpcnet_weights.c
  ${RN}/src/rnnoise_data.c ${RN}/src/rnnoise_tables.c
  ${RN}/src/x86/x86cpu.c ${RN}/src/x86/x86_dnn_map.c
  ${RN}/src/x86/nnet_sse4_1.c ${RN}/src/x86/nnet_avx2.c)
target_include_directories(rnnoise PUBLIC ${RN}/include PRIVATE ${RN}/src)
# Match upstream's normal build: debug-float weights are deliberately disabled.
target_compile_definitions(rnnoise PRIVATE RNN_BUILD DISABLE_DEBUG_FLOAT RNN_ENABLE_X86_RTCD OPUS_X86_MAY_HAVE_SSE OPUS_X86_MAY_HAVE_SSE2 _CRT_SECURE_NO_WARNINGS)
set_source_files_properties(${RN}/src/x86/nnet_avx2.c PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
set_source_files_properties(${RN}/src/x86/nnet_avx2.c PROPERTIES COMPILE_DEFINITIONS "__FMA__")
set_source_files_properties(${RN}/src/x86/nnet_sse4_1.c PROPERTIES COMPILE_DEFINITIONS "OPUS_X86_MAY_HAVE_SSE4_1")
add_library(speex_resampler STATIC ${SP}/libspeexdsp/resample.c)
target_include_directories(speex_resampler PUBLIC ${SP}/include PRIVATE ${SP}/libspeexdsp)
target_compile_definitions(speex_resampler PRIVATE FLOATING_POINT EXPORT= _USE_MATH_DEFINES)
