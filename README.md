# FXChronyDisplay For TTGO T-Display V1.1

1. Download and install Arduino:
https://www.arduino.cc/en/software

2. Get the TTGO T-Display TFT_eSPI library 

Go here: https://github.com/Xinyuan-LilyGO/TTGO-T-Display

Click the green code button and download the TTGO-T-Display zip

Open that .zip and extract the TFT_eSPI directory to you Arduino libs directory.

3. Get the OpenFontRender Library

Go here: https://github.com/takkaO/OpenFontRender

Click the green code button and download the OpenFontRender-master zip

Open that .zip and extract the folder to you Arduino libs directory. Rename it OpenFontRender

4. Get the sketch

https://github.com/DaystateRebel/FXChrony_TTGO_v1.1

Click the green code button and choose download Zip. Unzip the file somewhere

5. Start Arduino, click File | Open and navigate to the FXChrony_TTGO_v1.1 directory. 
Select the FXChrony_TTGO_v1.1.ino file

6. Install the One Button library
Click Sketch | Include Library | Manage Libraries ...
Use the "Filter your Search" to search for and install "onebutton"

7. Select the hardware: Tools | Boards Manager ... | esp32 | ESP32 Dev Module

8. Select the serial port your TTGO T-Display V1.1 is connected to

9. Click the Arrow button to build and flash the software
