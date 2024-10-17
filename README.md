# _KV Store for ESP32_

## Overview
This project is an ESP32-based data logger that reads temperature (and optionally humidity) measurements from a DHT11 sensor at regular intervals. It stores the measurements in a buffer and non-volatile storage (NVS). The ESP32 runs an HTTP server that allows users to query measurements by timestamp via a web interface or HTTP requests. The device connects to a Wi-Fi network, synchronizes time using SNTP, and uses mDNS for easy access over the network.



## Features
•	DHT11 Sensor Integration: Reads temperature data every minute.

•	Data Buffering: Stores measurements in a buffer and writes to NVS when the buffer is 90% full.

•	HTTP Server: Provides an API and web interface to query measurements by timestamp.

•	Web Interface: User-friendly web page to input timestamps and display results.

•	Wi-Fi Connectivity: Connects to a specified Wi-Fi network.

•	SNTP Time Synchronization: Synchronizes time using NTP servers.

•	mDNS Support: Access the device using a hostname instead of an IP address.

•	Configuration Management: Uses menuconfig for storing sensitive data and configurations.

## Hardware Requirements
•	ESP32 Development Board

•	DHT11 Temperature and Humidity Sensor

•	Jumper Wires

•	USB Cable (for programming and power)

•	Wi-Fi Network

## Software Requirements
•	ESP-IDF: ESP32 development framework (v4.x or above recommended)

•	Python 3.x: Required by ESP-IDF for build tools

•	Serial Monitor: idf.py monitor or any serial terminal program

## License
This project is licensed under the MIT License.

## Contributing
Contributions are welcome! Please follow these steps:

1.	Fork the repository.
2.	Create a new branch: git checkout -b feature/your-feature-name.
3.	Commit your changes: git commit -am 'Add new feature'.
4.	Push to the branch: git push origin feature/your-feature-name.
5.	Submit a pull request.

## Contact
For questions or support, please open an issue on the GitHub repository or contact the project maintainer at abhishek.karki@tum.de