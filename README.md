# Smart Helmet System 🚀

## Overview
The Smart Helmet System is an IoT-based safety solution designed to enhance rider safety by enforcing helmet usage, detecting accidents in real time, and sending emergency alerts with location data.

This system integrates multiple sensors, wireless communication modules, and microcontrollers to create a reliable, real-world safety mechanism for two-wheeler riders.

---

## Key Features

- Helmet detection-based ignition control system  
- Real-time accident detection using motion sensors  
- Emergency alert system with GPS location tracking  
- Wireless communication using RF modules  
- Alcohol detection for safety compliance  
- Automated response system using GSM module  

---

## Hardware Components

- Arduino UNO (Helmet Unit - Transmitter)  
- Arduino Nano (Vehicle Unit - Receiver)  
- MPU6050 (Accelerometer + Gyroscope for crash detection)  
- Neo-6M GPS Module (Location tracking)  
- SIM800L GSM Module (SMS alerts)  
- MQ-3 Alcohol Sensor  
- 433 MHz RF Transmitter & Receiver  
- Servo Motor (Ignition control simulation)  
- TP4056 (Battery charging module)  

---

## System Architecture

The system is divided into two main modules:

### 1. Helmet Unit (Transmitter)
- Detects helmet usage and rider condition  
- Uses RF transmitter to send status to vehicle unit  
- Monitors:
  - Motion (MPU6050)
  - Alcohol level (MQ-3)

### 2. Vehicle Unit (Receiver)
- Receives signal from helmet  
- Controls ignition using servo motor  
- Activates accident detection and alert system  
- Sends SMS alerts using GSM module with GPS location  

---

## How It Works

1. Helmet is worn → system activates  
2. MQ-3 sensor checks for alcohol presence  
3. MPU6050 continuously monitors motion and detects abnormal impact  
4. Helmet unit transmits status via RF module  
5. Receiver validates signal:
   - If helmet is worn → ignition enabled  
   - If not → ignition disabled  
6. In case of accident:
   - GPS fetches location  
   - GSM module sends alert message to predefined contacts  

---

## Code Structure

- `Transmitter.ino` → Handles helmet-side logic (sensors + RF transmission)  
- `Receiver.ino` → Handles vehicle-side logic (RF reception + ignition + alerts)  

---

## Applications

- Two-wheeler safety systems  
- Accident detection and emergency response  
- Smart transportation systems  
- IoT-based safety solutions  

---

## Project Highlights

- Multi-module embedded system (RF + GPS + GSM + sensors)  
- Real-world safety application with practical implementation  
- Presented at a National-Level Science Exhibition  
- Designed for low-latency and real-time response  

---

## Future Improvements

- Mobile app integration for live tracking  
- Cloud-based monitoring dashboard  
- AI-based accident prediction  
- Integration with vehicle ECU systems

## Author

Raj Priy
CSE @ VIT Bhopal  
