# Use the sounddevice module
# http://python-sounddevice.readthedocs.io/en/0.3.10/

import numpy as np
import sounddevice as sd
import time
import matplotlib.pyplot as plt
import serial
# Samples per second
sps = 2000

# Frequency / pitch
freq_hz = 50

# Duration
duration_s = 1
time_vector=np.linspace(0,duration_s,int(sps*duration_s),endpoint=False)

# Attenuation so the sound is reasonable
atten = 0.3

# NumpPy magic to calculate the waveform
each_sample_number = np.arange(duration_s * sps)
signal = 1 * np.sin(2* np.pi * freq_hz * duration_s)
waveform = np.sin(2 * np.pi * each_sample_number * freq_hz / sps)
waveform_quiet = waveform * atten


#print(waveform)

# Play the waveform out the speakers
sd.play(waveform_quiet, sps)
time.sleep(duration_s)
sd.stop()

#ser = serial.Serial('/dev/tty.usbserial', 9600, timeout=0.5)
#ser.write('*99C\r\n')
#time.sleep(0.1)
#ser.close()
# Configure the serial port
ser = serial.Serial(
    port='/dev/cu.Bluetooth-Incoming-Port',  # Specify the port name (may vary depending on your system)
    baudrate=9600,         # Set the baud rate
    timeout=20             # Set a timeout value (in seconds)
)

# Open the serial port
ser.open()

# Write data to the serial port
data_to_send = b'Hello, serial port!'
ser.write(waveform)

# Close the serial port
ser.close()

#plt.plot(time_vector,waveform)
#plt.show()