# SensESP Fan Control Project

Based on the standard template to be able to control a fan.  This will be used for exhaust fans on the boat and will allow for on off and speed control of pwm fans.  As well as measurements being sent to SignalK.

Comprehensive documentation for SensESP, including how to get started with your own project, is available at the [SensESP documentation site](https://signalk.org/SensESP/).


Hardware
This uses the Adafruit INA260 to measure power, and the EM2101 to control the fan through I2C.
The fan can be controlled by a momentary button (mine is on GPIO  35) as well as it will accept HTML PUT commands to set the fan speed.  Also a web page is provided at port 8081 to show the data from the unit and also set the fan speed.

