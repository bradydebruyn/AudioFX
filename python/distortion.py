import numpy as np
from scipy.io import wavfile
import datetime

#read input file
sample_rate, samples = wavfile.read('test.wav')


def clip_distortion(samples, threshold):
    clipped = np.copy(samples)  
    print("----------------------------------")
    print(f"Number of samples: {len(clipped)}")
    
    #print start time
    start_time = datetime.datetime.now()
    print(f"Start time: {start_time}")
    
    for i in range(len(clipped)):
        #has to handle different number of channels
        if clipped.ndim == 2:  #stereo
            for channel in range(clipped.shape[1]):
                if clipped[i][channel] > threshold:
                    clipped[i][channel] = threshold
                elif clipped[i][channel] < -threshold:
                    clipped[i][channel] = -threshold
        else:  #mono
            if clipped[i] > threshold:
                clipped[i] = threshold
            elif clipped[i] < -threshold:
                clipped[i] = -threshold
    
    #print end time and duration
    end_time = datetime.datetime.now()
    print(f"End time: {end_time}")
    print(f"Duration: {end_time - start_time}")
    print("----------------------------------")
    
    return clipped

distorted = clip_distortion(samples, 200)
wavfile.write('distorted_output.wav', sample_rate, distorted) #writes output to new wav file



