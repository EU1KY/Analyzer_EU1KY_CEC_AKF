# Note from EU1KY

DH1AKF passed away on March 19, 2021.
EU1KY is taking over his repo maintenance, now at the EU1KY GIT account since the original repo is not accessible for commits.
The code is cleaned in order to be built without warnings. Some bugs have been fixed, some improvements made.
I have no much time to deal with this code, but will add some requested features and fixes soon.

# antennaanalyzer
EU1KY Antenna Analyzer Firmware modified by DH1AKF and KD8CEC
Actual informations you can find at https://groups.io/g/Analyzer-EU1KY

Further Firmware information can be found in the link below.
http://www.hamskey.com

EU1KY site README.md
---------------------------------------------------
# README #

The EU1KY antenna analyzer V3 is an open source project to build your own, reasonably cheap but very functional antenna analyzer that is a handful tool for tuning coax-fed shortwave ham radio and CB antennas. Parameters are similar to known RigExpert AA-170, but color TFT LCD and some features outperform it in usability. Moreover, you have fun building this tool on your own and save some money.

See [Wiki](https://bitbucket.org/kuchura/eu1ky_aa_v3/wiki/Home) for details.

Other related sites: 

http://www.wkiefer.de/x28/EU1KY_AA.htm

http://ha3hz.hu/hu/home/top-nav/12-seged-berendezesek/12-eu1ky-antenna-analizator

Improvements (DH1AKF) against EU1KY's original software:
- Using the 5th harmonic of the oscillator enables frequencies up to  1450 MHz (or higher, hardware dependent)
- OSL calibration in steps of 100 kHz (till 150 MHz), in 300 kHz steps above 150 MHz
- Improved TDR
- Realtime clock, buzzer e.g. for acoustic SWR indicator and touch quit signal
- Battery control and notification
- Connection of a Bluetooth module is possible
- Multi band SWR measurement (max. 5 bands simultaneus)
- Screen shots and management of them
- Individual user screen with choosable showing time
- RF generator with (keyed) AM and FM
- Measurement of Quartz parameters
- Frequency sweep to find unknown pulsations ("Find Frequency", spectrum with power measurements)

More enhancements were programmed by KD8CEC:
- L/C measurement
- Measurement of |S21| with a separate oscillator output connection
- WSPR, FT8, FT4, JT65 transmittings for test purposes

## Contribution policy ##

If you have something to add to the source code, some feature or an improvement, please contact EU1KY (kuchura@gmail.com)

