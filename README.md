# Edge AI LoRa Anomaly Detection

![PlatformIO](https://img.shields.io/badge/PlatformIO-Supported-blue.svg)
![Framework](https://img.shields.io/badge/Framework-Arduino/C++-green.svg)
![Hardware](https://img.shields.io/badge/Hardware-Heltec%20V4%20|%20SX1262-orange.svg)
![Status](https://img.shields.io/badge/Status-Active%20Development-yellow.svg)
[![Web Flasher](https://img.shields.io/badge/Web%20Flasher-Install%20via%20Browser-brightgreen?logo=googlechrome&logoColor=white)](https://albertokeroro.github.io/edge-ml-anomaly-detector-flasher/)
[![Edge Impulse](https://img.shields.io/badge/Edge%20Impulse-Model%20%26%20Dataset-blueviolet)](https://studio.edgeimpulse.com/public/900708/latest)

An advanced, edge-computing firmware designed to safeguard decentralized sub-GHz mesh communication networks (such as Meshtastic) against malicious physical-layer RF manipulation. 

This project implements a lightweight Machine Learning classification pipeline built for constraints-restricted microcontrollers used to detect, identify, and alert on active radio frequency interference profiles in real-time.

## 📺 Interactive Showcase & Extra Documentation

For a comprehensive breakdown of the hardware architecture, algorithmic performance metrics, and live field-testing demonstrations, check out the interactive portfolio showcase.

* **View the Showcase:** **[Portfolio Deep Dive & Demo](https://jalbertomoro.netlify.app/?project=rf-detector)**

## 🚀 Browser-Based Deployment

Pre-compiled production binaries can be flashed directly to the target hardware via a web browser without the need to compile.

* **Direct Installation:** Access the interface at the **[Web Flasher Utility](https://albertokeroro.github.io/edge-ml-anomaly-detector-flasher/)**.

## 🧠 Machine Learning Model & Dataset

The neural network architecture, DSP feature extraction blocks, and the raw RF training dataset used for this pipeline are publicly accessible. You can explore the spectral features and test the model's accuracy directly in your browser.

* **Explore the Data:** **[Edge Impulse Public Project](https://studio.edgeimpulse.com/public/900708/latest)**

## Highlights

* **Ultra-Low Memory Footprint Anomaly Inference:** Runs real-time local spectral analysis in a fast **1ms inference time** utilizing only **1.4KB of peak RAM**, leaving maximum resources open for host network stacks.
* **AGC-Resilient Adaptive Tracking Squelch:** Features a mathematical tracking engine that filters out SX1262's AGC's abrupt gain gear-shifts to prevent false-positive alarms while maintaining max sensitivity.
* **Multi-Profile RF Threat Simulation:** Includes isolated validation utilities mirroring real-world attack vectors (Continuous Wave, Symmetric/Asymmetric Pulsed, Packet Floods, and Preamble-Stun sequences).
* **Dual-Mode Visual Firmware UI:** Features a dedicated Edge ML jamming detection display, complete with a persistent historical alarm latch, automatic dynamic thresholding, and a real-time RSSI graph.

## Repository Architecture

This project is structured as an intentional, modular monorepo. Each sub-project isolates its compiler tasks to allow seamless development in PlatformIO without dependency bleeding.
