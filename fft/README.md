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


### MQTT SSL connection

The project connect to mqtts://mqtt.eclipseprojects.io:8883 and uses the pem certificate of the broker in order to connect to it.
This configuration can be changed in fft/main/Kconfig.projbuild.
In order to use an SSL connection you should retrieve the certificate of the server and put it inside the mqqt_eclipseprojects_io.pem file.
