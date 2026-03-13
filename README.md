Bitcrusher.cpp, Echo.cpp, and Distortion.cpp all take single samples and implement their respective effects on the sample.

All header files are used in the wav_processor.cpp. Run the testbench in Vitis HLS adding headers and cpp files as design sources. wav_processor file uses some StackOverflow references to properly recieve WAV files as an input and then (depending on selection made in the code) applies the desired effects to the audio file and saves them to a new file. Input and output file paths must be specified. A sample WAV is provided. 
