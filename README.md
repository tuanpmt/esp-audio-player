#ESP8266 Audio Player

Based on: 
- [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos)
- mad library from https://github.com/espressif/ESP8266_MP3_DECODER/
- http://kissfft.sourceforge.net/
- Read mp3 file from spiffs (internal ESP8266 storage)

## Demo: 
![DemoImage](./demo.png)

https://youtu.be/8Z-Cp0hWlhs

## Install
- Put `mp3` mono file `happy-new-year.mp3` to the folder `files` (file size limit is 2Mbytes)

```
export OPEN_RTOS_PATH=/path/to/esp-open-rtos
make && make flash
```

- I2S connection (PCM5102 or any I2S DAC)
```
ESP pin   - I2S signal
----------------------
GPIO2/TX1   - LRCK
GPIO3/RX0   - DATA
GPIO15      - BCLK
```

- OLED connection (SH1106 or SSD1306) - I2C
```
ESP pin     - OLED
----------------------
GPIO4       - SDA
GPIO5       - SCL
```

- Hardware for test: [iot-wifi-uno](https://github.com/iotmakervn/iot-wifi-uno-hw)

![iot-wifi-uno](https://github.com/iotmakervn/iot-wifi-uno-hw/raw/master/assets/Iot-wifi-uno-hw-pinout.png)

## License 

- MIT license for this application
