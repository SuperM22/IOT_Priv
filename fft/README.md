# Signal processor simulator 

This project demonstrates how to perform an FFT of an input signal, process the maximum frequency of the signal, re-sample the input signal at the optimal sampling frequency and transimts data of the signal through an MQTT server (SSL).

## How to use this project

### Hardware required

This project was built to be run on an ESP32S3 v3 developement board.

### Input the signal

The signal sampling is simulated inside the code.
We assume that the signal will be a sum of sinusoids of the form of SUM(a_k*sin(f_k)).
In order to inject the components of the signal into the code, before building the project, you can modify this instance:

```
SignalComponent component = {
    2, // Number of sine components
    {3,5}, // Sine frequencies in HZ
    {2,4}, // Sine amplitudes
};
```
### Aggregate of the signal
The signal aggregate is a simple mean calculated over a certain time window, to specify the time window just set:

```
#define WINDOW_TIME 5 //seconds
```

### MQTT SSL connection

The project connect to mqtts://mqtt.eclipseprojects.io:8883 and uses the pem certificate of the broker in order to connect to it.
This configuration can be changed in fft/main/Kconfig.projbuild.
In order to use an SSL connection you should retrieve the certificate of the server and put it inside the mqqt_eclipseprojects_io.pem file.
The topic chosen is /topic/qos0.

## Project info
### Signal sampling
The signal sampling is simulated throughout this function:

```
void signal_sampling(){
        for (int i = 0; i < N; i++) {
        float t = (float)i / SAMPLING_RATE;
        wave[i] = composite_signal(t, &component);
        
    }
    ESP_LOGI("signal_sampling", "Signal sampling over ");
}

```
As you can see the sampling is ideal, so the sample is equal to the signal at the sampling instant.
In order to have a more realistic approach you can add a random amount to t every time the signal is sampled.

### FFT Process
The FFT calculation is done troughout the dsp library for esp-idf.
Always make sure that the number of samples are expressible as a power of 2.
You can obtain more information about it [here](https://github.com/espressif/esp-dsp).

### Optimal frequency of the input signal

We determine the optimal frequency of the signal as:
```
    for (int i = 0; i < N; i++) 
    {
        float mag = sqrt( y_cf[i]*y_cf[i] + y_cf[i+1] * y_cf[i+1]);
        if(mag>TH && mag>maxM){
            maxI = i;
            maxM = mag;
        }
    }
    float maxF = maxI;
    maxF = maxF*(SAMPLING_RATE) / (float) (N*2);

```
That will result in the index that has the highest magnitude in the resulting array.<br>
Note that this is divided by 2N since the FFT result size is twice N, number of samples.<br>
Why do I iterate just N times then ?<br>
Note that for a real input signal (imaginary parts all zero) the second half of the FFT (bins from N / 2 + 1 to N - 1) contain no useful additional information (they have complex conjugate symmetry with the first N / 2 - 1 bins). The last useful bin (for practical aplications) is at N / 2 - 1. The bin at N / 2 represents energy at the Nyquist frequency, but this is in general not of any practical use, since anti-aliasing filters will typically attenuate any signals at and above Fs / 2.

###Correct initial sampling rate
Since we are actually simulating the sampling of the signal, we actually already know what maximum frequency it will have.<br>
This lets us choice an initial sampling frequency that will not yeld 0Hz as the optimal frequency as a result.<br>
But why would the FFT return a spike in the first bin frequency?<br>
The first bin in the FFT is DC (0 Hz), the second bin is Fs / N, where Fs is the sample rate and N is the size of the FFT. The next bin is 2 * Fs / N. To express this in general terms, the nth bin is n * Fs / N.
So if your sample rate, Fs is say 44.1 kHz and your FFT size, N is 1024, then the FFT output bins are at:
```
  0:   0 * 44100 / 1024 =     0.0 Hz
  1:   1 * 44100 / 1024 =    43.1 Hz
  2:   2 * 44100 / 1024 =    86.1 Hz
  3:   3 * 44100 / 1024 =   129.2 Hz
  4: ...
  5: ...
```
So if we input a very low frequency signal, and initially sample it at a very high frequency we will yeld 0Hz as the maximum frequency component of the input signal.<br>
That is why in the code I'm initially sampling at 4KHz.

### Averaging and transmitting to the MQTT server
After we perform the FFT we have the maximum frequency of the signal. We then proceed to get the correct sampling frequency that will give us all the information about the input signal, following the [Nyquist–Shannon sampling theorem](https://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem).<br>
Then I resample the signal, and process it mean value.
The mean value is then transmitted over MQTT.
