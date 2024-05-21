# IOT_Priv
Personal project for IOT course, MECS La Sapienza.
## Setup and run the project
1. Download the project.
2. Clean the project build (idf.py clean or manually delete the build folder).
3. Correctly setup the Wi-Fi connection and the flash size of your device (idf.py menuconfig in the fft folder): Serial flasher config -> Flash size (8MB), Example Connection -> Input your access point id and password.
4. Connect your device.
5. Run idf.py build flash monitor.


More info can be found in fft and power consumption folders.
