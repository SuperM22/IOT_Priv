# IOT_Priv
Individual project for IOT Algorithms and Services course, Master's degree in ECS La Sapienza, Rome.
This project was made by [me](https://www.linkedin.com/in/domenico-lattari-0947b9225/)

# Signal processing 

This project demonstrates how to perform an FFT of an input signal, process the maximum frequency of the signal, re-sample the input signal at the optimal sampling frequency and transimts data of the signal through an MQTT server (SSL). This project was built to be run on an ESP32S3 v3 developement board.
It's also included power consumption monitoring (oversampling vs optimal sampling frequency), network usage monitoring and latency monitoring of the whole system.

## How to run

1. Download the project.
2. Clean the project build (idf.py clean or manually delete the build folder).
3. Correctly setup the Wi-Fi connection and the flash size of your device (idf.py menuconfig in the fft folder): Serial flasher config -> Flash size (8MB), Example Connection -> Input your access point id and password.
4. Connect your device.
5. Run idf.py build flash monitor. <br>
To stop monitoring press ```Ctrl+T + Ctrl+X ``` inside the terminal window.

## Project setup

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
Note that for a real input signal (imaginary parts all zero) the second half of the FFT (bins from N / 2 + 1 to N - 1) contain no useful additional information (they have complex conjugate symmetry with the first N / 2 - 1 bins). The last useful bin (for practical aplications) is at N / 2 - 1. The bin at N / 2 represents energy at the Nyquist frequency, but this is in general not of any practical use, since anti-aliasing filters will typically attenuate any signals at and above Fs/2.<br>

### Correct initial sampling rate

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

### Network Data measurement
In order to perform the amount of data transmitted to the network I have used [Wireshark](https://www.wireshark.org/download.html).<br>
The correct way to do that is to connect the ESP32 and the pc on which we are using wireshark to the same WiFi access point.
After this is done, when the ESP32 connects throughout the internet, it declares its ip in the code:
```
I (22285) example_common: - IPv4 address: 172.20.10.3
```
In normal cases you should be able to apply a filter in wireshark with that ip address and direcly check for the outgoing data.
In my particular case there was a problem, and I could not see any traffic to this IP address.
So I connected an MQTT app to the broker, on the same topic with the same kind of connection, and sent the same message that the ESP was sending.
My ESP was actually receiving the messages but I still could not see any traffic on that IP.
So I directly analysed the traffic of the app. All the informations are correct since the protocol used is TLS v1.2 and the destination port is 8883.
And what comes out is that the packet that I send through the MQTT server, consisting in a single string:
```
TOPIC=/topic/privIot
DATA=The average of the signal is: -0.0000001
```
actually weights 144 bytes (there are more than 1 since I purposely spammed the message in order to create a bigger number of packets): <br>
![Schermata 2024-05-22 alle 10 15 25](https://github.com/SuperM22/IOT_Priv/assets/62383917/06c57a0d-f127-4b06-abfc-8dbea3bfadc9) <br>

Since we are subscribed to the topic the packet is sent and then received.

### System latency evaluation
The latency evaluation of the system is calculated by  getting the ticks at specific points inside the code by using xTaskGetTickCount(), and then getting the final time of the application by:
```
end_time = publish_time-counter_init;
end_time = (unsigned int) (end_time * portTICK_PERIOD_MS);
ESP_LOGI("MQTT","The total latency is %ld milliseconds",end_time);
```
The timer is started before the generation of the signal and ended as soon as we publish the data on the MQTT server. <br>
The total latency of the system varies due to some coiches in the code:
- if we connect to the WiFi before starting the timer (so we do not consider the wifi connection time), the total latency is around 2050ms.
- if we consider the wifi connection, the total latency incrases to 20 seconds.
In those extimation power consumption is not considered, since in the code there are some ```vTaskDelay(1000);``` in order to perform more accurate power consumption readings.

### Power consumption monitoring

In order to correctly evaluate the power consumption we need:
1. An arduino board (I've used an arduino mega2560)
2. An INA219
3. Breadboard and wires
4. USBC and ducktape

In order to perform the connection:
- INA219 VCC to Arduino 5v
- INA219 GND to Arduino GND
- INA219 Scl to Arduino Scl
- INA219 Sda to Arduino Sda
- INA219 Vin+ to Arduino 5v
- INA219 Vin- to ESP32 5v
- ESP32 GND to Arduino GND

To start the code you can inject the code you find in the power consumption folder into the arduino board and open the serial monitor (set the baud rate to 115200).
To monitor the power consumption in determined points of the code (signal sampling and resampling), after alimenting the ESP32 with this circuit you can connect it through the USB cable and monitor it.
Since the ESP32 has a mutual exclusive alimentation, in order to monitor it we can just tape the 5v of the usb cable we use it to connect to the PC.
It is a really simple process, [here](https://community.octoprint.org/t/put-tape-on-the-5v-pin-why-and-how/13574) u can see how it is done.
At this purpose I've introduced some logs and ```vTaskDelay(1000);``` right before and after the signal ampling and resampling.
Here are the result of the powerconsumption measurement:
- this is the power consumption when we initially oversample the signal:<br>

![Schermata 2024-05-22 alle 11 31 42](https://github.com/SuperM22/IOT_Priv/assets/62383917/cf821fee-2fff-4ba6-92f2-78a9c4b67bb4)

- this is while we resample the signal at an optimal sampling frequency:<br>

![Schermata 2024-05-22 alle 11 33 14](https://github.com/SuperM22/IOT_Priv/assets/62383917/b7be4c8c-562b-47c6-a0cb-cbf3ae8aabce)

Since I am simulating the sampling of the signal inside the code itself, it takes a really short time to be sampled, and an even shorter time to be resampled. This makes it really hard to monitor.

