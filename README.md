Bitcrusher.cpp and Distortion.cpp both take single samples and implement their respective effects on the sample.

Both header files are used in the wav_processor.cpp. This file uses some StackOverflow references to properly recieve WAV files as an input and then 
(depending on selection made in the code) applies the effects to the audio file and saves them to a new file. Input and output file paths must be specified. A sample WAV is provided. 
