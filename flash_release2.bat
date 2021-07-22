@rem This batch file runs ST-Link utility to flash the Release firmware to the device
"c:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -d bin\release\F7Discovery.bin 0x08000000 
pause