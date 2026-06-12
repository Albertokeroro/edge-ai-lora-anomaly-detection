# Edge AI LoRa Anomaly Detection

![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-blue.svg)
![Framework](https://img.shields.io/badge/Framework-Arduino/C++-green.svg)
![Hardware](https://img.shields.io/badge/Hardware-Heltec%20V4%20|%20SX1262-orange.svg)
![Status](https://img.shields.io/badge/Status-Active%20Development-yellow.svg)

An advanced, edge-computing firmware designed to safeguard decentralized sub-GHz mesh communication networks (such as Meshtastic) against malicious physical-layer RF manipulation. 

This project implements a lightweight Machine Learning classification pipeline directly on constraints-restricted microcontrollers to detect, identify, and alert on active radio frequency interference profiles in real-time.

---

## Highlights

* **Ultra-Low Memory Footprint Anomaly Inference:** Runs real-time local spectral analysis in a blistering **1ms inference time** utilizing only **1.4KB of peak RAM**, leaving maximum silicon resources open for host network stacks.
* **AGC-Resilient Adaptive Tracking Squelch:** Features an advanced mathematical tracking engine that filters out internal Semtech SX1262 hardware gain gear-shifts to prevent false-positive alarms while maintaining max sensitivity.
* **Multi-Profile RF Threat Simulation:** Includes isolated validation utilities mirroring real-world attack vectors (Continuous Wave, Symmetric/Asymmetric Pulsed, Packet Floods, and Preamble-Stun sequences).
* **Dual-Mode Visual Firmware UI:** edge ML jamming detection screen with a a persistent historical alarm latch and and automatic dynamic threshold and a realtime rrsi graph.

---

##  Repository Architecture

This project is structured as an intentional, modular monorepo. Each sub-project isolates its compiler tasks to allow seamless development in PlatformIO without dependency bleeding.
