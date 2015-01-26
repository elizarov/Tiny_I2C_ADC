I2C ADC Based on ATTiny85
=========================

This is a cheap 10-bit ADC with I2C interface based on ATTiny85 chip.
Use ATTiny85 with default fuses (1MHz internal clock). 
See tiny_i2c_adc.cpp for hardware connections layout.

Use it with I2C address 'A' and read two bytes: HSB and LSB of 10-bit ADC reading.
This code is fully asyncronous puts CPU into power-down mode all the time, so the power consumption is very low (<1uA)
