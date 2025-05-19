# Sabiana CVB-T Smart Heat Pump Integration Mod

This project modifies the Sabiana CVB-T convector to better integrate with intelligent heat pump systems, such as those from Nibe.

Modern heat pumps optimize energy use by only enabling the heating water supply line when heating is needed. However, the default CVB-T behavior does not detect this. It periodically starts up to check for hot water, regardless of whether any is available — leading to unnecessary fan operation and inefficiency.

## 🔧 Key Features

- **Intelligent Start Logic**  
  The CVB-T will only activate when hot water is available, thanks to added logic that synchronizes with the heat pump's flow activation. This improves efficiency and reduces wear.

- **HMI Touch Interface**  
  A modern touch-based Human-Machine Interface (HMI) is added, allowing for easier monitoring and manual control of the system.

## 🛠️ Hardware Overview
This project introduces a small custom ESP-based PCB that plugs in between the Sabiana CVB-T main PCB and its front panel (which includes the status LEDs and IR receiver). This board acts as an interceptor and controller, enabling smarter control and communication with a new HMI touch panel over UART.

![HW diagram](images/cvbt_mod.svg)

The interceptor board receives IR commands via the existing CVB-T IR receiver. It also controls the original CVB-T status LEDs and sends commands to the CVB-T using its Modbus interface.