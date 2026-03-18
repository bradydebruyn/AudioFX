Project Group: Fausta Fenner and Brady Debruyn

Echo.cpp, Bitcrusher.cpp, and Distortion.cpp all take single samples and implement their respective effects on the sample.

All header files are used in the wav_processor.cpp. This file uses some StackOverflow references to properly recieve WAV files as an input and then
(depending on selection made in the code) applies the effects to the audio file and saves them to a new file. Input and output file paths must be specified. A sample WAV is provided.

To select what effects you would like to apply, change the EffectMode variable within wav_processor.cpp.

We originally tested our initial concepts in Python, before moving to c++ to implement distortion, bit crusher, and echo.
