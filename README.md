I2C ADC Based on ATTiny85
=========================

This is a cheap 10-bit ADC with I2C interface based on ATTiny85 chip.
Use ATTiny85 with default fuses (1MHz internal clock). 
It should work with ATTiny25 and ATTiny45, too (it need less than 500 bytes of flash and a virtually no SRAM).
See tiny_i2c_adc.cpp for full hardware connections layout.

The voltage on INPUT (ADC2, pin3) is measured against VCC and is reported as 10-bit value from 0 to 1023.

Use it as I2C slave with I2C address 'A':

 * Master must send first byte (('A' << 1) & 1) to start reading.
 * Master must read two bytes: HSB and LSB of ADC result.

The address byte initiates conversion and the clock is streched while conversion is in progress.

This code is fully asyncronous and interrupt-driven. It puts CPU into power-down mode all the time, 
so the power consumption is extremely low.
