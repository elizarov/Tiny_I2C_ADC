@if [%AVR_HOME%] == [] echo Set AVR_HOME environment variable to the home of AVR GCC toolchain
@SET PATH=%AVR_HOME%\bin;%PATH%
@make