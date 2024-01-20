# SCPI Commands

This file lists the SCPI commands implemented in the firmware.

Mandated and required SCPI commands are recognised as well,
but may not perform as expected.


## *RCL {preset}

Restore a saved preset. **preset** must be between 1 and 9.


## SYSTem:ERRor[:NEXT]?

Returns and at the same time deletes the oldest entry in the error queue.


## SYSTem:ERRor:COUNt?

Returns the number of entries in the error queue.


## SYSTem:VERSion?

Returns the version of SCPI (Standard Commands for Programmable Instruments)
that the instrument complies with.


## DISPlay:BRIGhtness {level}

Set the display's brightness level. **level** must be between 0 and 5.


## DISPlay:BRIGhtness?

Returns the display's brightness level.


## DISPlay:LANGuage {language}

Set the front panel language. **language** is either
a number between 0 and 4 or **ENGLISH**, **CHINESE**,
**GERMAN**, **FRENCH** or **RUSSIAN**.


## DISPlay:LANGuage?

Returns the front panel language.


## SYSTem:DATE {year} {month} {day}

Set the power supply date.


## SYSTem:DATE?

Returns the power supply date.


## SYSTem:TIME {hour} {minute} {second}

Set the power supply time of day.


## SYSTem:TIME?

Returns the power supply time of day.


## OUTPut[:STATe] {0 | 1 | on | off}

The output state.


## OUTPut[:STATe]?

Returns the output state.


## OUTPut:MODE?

The output mode, either CV or CC.


## [SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude] {voltage}

Set the output voltage.


## [SOURce]:VOLTage[:LEVel][:IMMediate][:AMPLitude]?

Returns the output voltage.


## [SOURce]:VOLTage:PROTection:TRIPped?

Returns whether the OVP is tripped.


## [SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude] {current}

Set the output current.


## [SOURce]:CURRent[:LEVel][:IMMediate][:AMPLitude]?

Returns the output current.


## [SOURce]:CURRent:PROTection:TRIPped?

Returns whether the OCP is tripped.


## MEASure[:SCALar]:VOLTage[:DC]?

Returns the measured output voltage.


## MEASure[:SCALar]:CURRent[:DC]?

Returns the measured output current.


## MEASure[:SCALar]:POWer[:DC]?

Returns the measured output power.


## MEASure[:SCALar]:TEMPerature[:THERmistor][:DC]? {SYSTEM | PROBE}

Returns the system or probe temperature.


## [SOURce]:VOLTage:LIMit {voltage}

Set the Over-Voltage Protection value.


## [SOURce]:CURRent:LIMit {current}

Set the Over-Current Protection value.


## SYSTem:BEEPer:STATe {0 | 1 | on | off}

Control the buzzer.


## SYSTem:BEEPer:STATe?

Returns the buzzer state.
