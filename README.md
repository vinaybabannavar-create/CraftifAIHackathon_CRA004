# Workplace Safety Sentinel
### CraftifAI Physical AI Hackathon — PipeGen Track A

## Overview
Workplace Safety Sentinel is an edge-AI prototype exploring video-based
safety monitoring using NVIDIA DeepStream and the TrafficCamNet model.

## Goal
Help safety teams identify potentially unsafe activity from camera footage
and support future restricted-zone monitoring and incident reporting.

## Current Implementation
- Input: video file
- Inference: TrafficCamNet ONNX model with a TensorRT engine
- Runtime: NVIDIA DeepStream 7.1
- Output: generated MP4 video
- Configuration and screenshots: included in this repository

## Validation Status
The DeepStream pipeline loaded its TensorRT engine and processed the input
video. However, the detection overlays were incorrect, showing scattered
green blocks and overlapping boxes. Detection accuracy is not yet validated.

Person tracking, restricted-zone alerts, JSON incident logging, and automated
notifications are not claimed as completed features.

## Repository Contents
- `config/`: inference and post-processing configuration
- `evidence/`: screenshots of the observed output issue

## Environment
- Ubuntu on WSL2
- Windows 11 host
- NVIDIA GeForce RTX 3050
- Docker
- NVIDIA DeepStream 7.1

## Limitations
This is a prototype tested with a street video, not verified industrial-site
footage. The detection visualization requires further debugging before the
system can be considered operational.

## Next Steps
1. Correct model preprocessing and output decoding.
2. Validate detection boxes and labels.
3. Implement and test restricted-zone event detection.
4. Add verified JSON incident output.
5. Complete the PipeGen export and required Pipeline IR evidence.
