# soft-shark-robot
ESP32-S3 firmware and native iOS app to control a bio-inspired Soft Shark Robot.

## Xiao ESP32S3 Firmware
The robot firmware runs on the Seeed Studio XIAO ESP32S3 microcontroller.

### Requirements:
* Arduino IDE version 2.0.15 or earlier.
* ESP32 board package by Espressif Systems, version 2.0.15 or earlier.
* Seeed Studio XIAO ESP32-S3 board support.

### Setup and Upload:
* Download and install Arduino IDE version 2.0.15 or earlier.
* Open Boards Manager and install the ESP32 platform by Espressif Systems, selecting version 2.0.15 or earlier.
* Open the firmware file located at Soft_Shark_Robot/Soft_Shark_Robot.ino.
* Connect the XIAO ESP32-S3 to your computer via USB.
* In Tools → Board, select XIAO ESP32S3.
* Select the corresponding USB port.
* Click Upload to compile and upload the firmware to the board.

## iOS App
The Soft Shark Robot includes an iOS application that allows users to interact with and control the robot through BLE.

### Requirements:
* Xcode installed on your Mac or MacBook.
* An iPhone and a USB cable.
* Your Apple Account.

### Installation and Setup:
* Download and install Xcode.
* Download this repository.
* Navigate to iOS/Shark Robotic Fish/.
* Open Robotic Fish.xcodeproj with Xcode.
* Connect your iPhone to your Mac via USB.
* Enable Developer Mode on your iPhone.
* In Xcode, select your iPhone as the build destination.
* Under Signing & Capabilities, select your personal Apple Account as the development team.
* Build and run the application by clicking the Run button.

Important: A paid Apple Developer Program membership is not required to build and install the application on your personal iPhone. However, free provisioning has certain limitations, including the need to renew the app signature periodically.