<div align="center">

# 🛡️ Workplace Safety Sentinel

### Edge-AI Video Monitoring Prototype

**CraftifAI PipeGen Hackathon · Participant ID: CRA004**

A prototype exploring video-based safety monitoring with NVIDIA DeepStream, TensorRT, and TrafficCamNet.

![DeepStream](https://img.shields.io/badge/NVIDIA-DeepStream%207.1-76B900?style=for-the-badge&logo=nvidia&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Ubuntu%20%2F%20WSL2-333333?style=for-the-badge&logo=linux&logoColor=white)
![Status](https://img.shields.io/badge/Status-Prototype%20%7C%20Needs%20Validation-orange?style=for-the-badge)

</div>

---

## 📌 Overview

Workplace Safety Sentinel is an edge-AI prototype exploring how video analytics could support workplace safety monitoring. The intended direction is to help safety teams review camera footage, identify people in monitored areas, and eventually flag potential restricted-zone entry for human review.

For this hackathon attempt, a video file was processed through NVIDIA DeepStream using the TrafficCamNet ONNX model and a TensorRT engine. The pipeline produced an output MP4, but the detection overlays were visibly incorrect. This repository records the work performed, the configuration used, the observed issue, and the next steps needed to make the pipeline reliable.

> **Transparency:** This is not a validated safety system. Person tracking, restricted-zone entry events, and structured incident JSON have not been verified as working. The test footage is a street video, not industrial workplace footage.

## 🎯 Intended goals

- Explore edge inference for video-based monitoring.
- Investigate how object detection could support safety workflows.
- Establish a foundation for future restricted-zone monitoring and incident review.
- Document model-integration and post-processing issues encountered during development.

## 🧱 Pipeline overview

```text
Input video
    ↓
Video decoding
    ↓
Frame preparation / model preprocessing
    ↓
TrafficCamNet inference (DeepStream + TensorRT)
    ↓
Detection post-processing
    ↓
Overlay rendering and video encoding
    ↓
Output MP4 + visual evidence
```

This is the intended processing path used in the experiment. The current detection visualization is incorrect and requires debugging before detections can be trusted.

## 🛠️ Technology stack

| Component | Technology / details |
|---|---|
| Edge inference | NVIDIA DeepStream 7.1 |
| Detection model | TrafficCamNet, ResNet-18 ONNX |
| Inference engine | TensorRT engine loaded by DeepStream |
| Video processing | GStreamer / DeepStream elements |
| Runtime environment | Ubuntu on WSL2, Windows 11 host |
| GPU | NVIDIA GeForce RTX 3050 (6 GB) |
| Container runtime | Docker with NVIDIA Container Toolkit |
| Post-processing | DeepStream `nvdspostprocess` configuration |

## 🖥️ Tested environment

- **Host OS:** Windows 11
- **Linux environment:** Ubuntu on WSL2
- **GPU:** NVIDIA GeForce RTX 3050 (6 GB)
- **Container name:** `craftifai-amd64-v3`
- **Container image:** `craftifaidocker/deepstream:7.1-amd64-v3`
- **DeepStream:** 7.1
- **Model:** `resnet18_trafficcamnet_pruned.onnx`
- **Model input shape observed:** `[batch, 3, 544, 960]`
- **Output tensors observed:** coverage tensor and bounding-box tensor

These are environment details, not a performance benchmark.

## 📂 Repository structure

```text
CraftifAIHackathon_CRA004/
├── README.md
├── .gitignore
├── config/
│   ├── trafficcamnet_detector.yml
│   └── trafficcamnet_infer_post.txt
└── evidence/
    ├── trafficcamnet_frame.jpg
    └── trafficcamnet_post_frame.jpg
```

- `config/trafficcamnet_detector.yml` — post-processing configuration used in the experiment.
- `config/trafficcamnet_infer_post.txt` — DeepStream inference configuration used for the post-processing test.
- `evidence/trafficcamnet_frame.jpg` — frame from the earlier output attempt.
- `evidence/trafficcamnet_post_frame.jpg` — frame from the later output attempt showing the visualization issue.

Generated MP4 files, the model engine, and the input video are not committed. Use video you have permission to process.

## 🚀 Reproduction notes

The following documents the environment and artifacts used during the test. This repository is **not yet a complete, one-command runnable application**.

### 1. Prerequisites

- Windows 11 with WSL2 and Ubuntu, or a compatible Linux host.
- A supported NVIDIA GPU and working NVIDIA driver.
- Docker with NVIDIA Container Toolkit configured.
- Access to a compatible NVIDIA DeepStream 7.1 container.
- A video file you have permission to use.

Check whether the container can access your GPU using suitable CUDA/NVIDIA tooling available in your environment.

### 2. Check the container

The experiment used a container named `craftifai-amd64-v3`:

```bash
docker ps --filter name=craftifai-amd64-v3
```

Replace the name in subsequent commands if your container is named differently.

### 3. Input video

The original test video was located at:

```text
/home/preet/sentinelai/videos/854100-hd_1920_1080_25fps.mp4
```

This is a path from the original development machine and will not exist on every system. Copy your own permitted video into the container and update the source URI accordingly.

### 4. Model and engine paths

The model used during the experiment was located at:

```text
/opt/nvidia/deepstream/deepstream-7.1/samples/models/Primary_Detector/resnet18_trafficcamnet_pruned.onnx
```

The TensorRT engine loaded by the test was:

```text
/opt/nvidia/deepstream/deepstream-7.1/samples/models/Primary_Detector/resnet18_trafficcamnet_pruned.onnx_b1_gpu0_fp32.engine
```

These files come from the container's DeepStream sample installation and are not included in this repository. Confirm they exist in your environment before attempting to run the pipeline.

### 5. Review the configuration

```bash
cat config/trafficcamnet_infer_post.txt
cat config/trafficcamnet_detector.yml
```

Preprocessing, tensor layout, bounding-box decoding, class labels, thresholds, and plugin configuration must match the model and installed DeepStream version.

### 6. Post-processing library build used in the experiment

The following command was used inside the original container when the post-processing library was missing:

```bash
docker exec -it craftifai-amd64-v3 bash -lc '
cd /opt/nvidia/deepstream/deepstream-7.1/sources/gst-plugins/gst-nvdspostprocess/postprocesslib_impl &&
export CUDA_VER=12.6 &&
make -j2
'
```

`CUDA_VER=12.6` reflects the original environment. Use the CUDA version supported by your own DeepStream installation rather than assuming this is portable.

### 7. Running status

The experiment used a GStreamer pipeline containing `uridecodebin`, `nvstreammux`, `nvinfer`, `nvdspostprocess`, `nvdsosd`, video conversion, H.264 encoding, and MP4 muxing. DeepStream loaded the TensorRT engine and an output MP4 was created.

However, the current repository does not include a complete runner script or a verified portable run command. The model output decoding and visual overlays must be corrected before the output can be treated as valid detections.

## 📊 Validation results

| Check | Observed result |
|---|---|
| NVIDIA GPU visible in development environment | Passed |
| DeepStream 7.1 container available | Passed |
| TensorRT engine loaded by `nvinfer` | Passed |
| GStreamer pipeline reached end-of-stream | Passed |
| Output MP4 created | Passed |
| Detection overlays visually correct | **Failed — requires debugging** |
| Person tracking validated | Not verified |
| Restricted-zone entry events validated | Not verified |
| JSON incident metadata validated | Not verified |
| FPS / latency benchmark | Not measured |

No accuracy, FPS, latency, or safety-performance figures are claimed.

## 🐛 Known issue and evidence

The generated output frames contain scattered green blocks and overlapping or incorrect bounding-box overlays. Loading the TensorRT engine and creating an MP4 does not prove that the model output was decoded and rendered correctly.

Evidence is available in `evidence/`:

- `trafficcamnet_frame.jpg`
- `trafficcamnet_post_frame.jpg`

### Investigation performed

- Inspected the TrafficCamNet ONNX model and its output tensor names.
- Reviewed the DeepStream inference and post-processing configuration.
- Built the DeepStream post-processing library in the container.
- Ran the pipeline and confirmed an output MP4 was produced.
- Extracted frames for visual inspection and documented the incorrect overlays.

### Next debugging steps

1. Verify the model's exact preprocessing and normalization requirements.
2. Confirm output tensor layout, bounding-box decoding, class mapping, and thresholds.
3. Compare the custom configuration with the configuration supplied for the installed model.
4. Validate boxes on known frames before enabling downstream logic.
5. Only after detection is correct, implement and test tracking and restricted-zone events.
6. Add structured incident output and test it against known scenarios.

## 🗺️ Planned improvements

- Correct and validate TrafficCamNet post-processing.
- Add person tracking with stable object IDs.
- Configure a user-defined restricted-zone polygon.
- Detect and test entry events for tracked people crossing into that zone.
- Write verified incident records to JSON with timestamps and zone/object details.
- Add a repeatable runner script and dependency checks.
- Capture a clean Pipeline IR graph and genuine end-to-end demo after validation.
- Measure FPS and latency under a documented test setup.

These are planned features, not completed functionality.

## 🔐 Safety and responsible use

- Process video only when you have permission.
- Treat detections as fallible and require human review before acting.
- Do not use this prototype as the sole basis for safety-critical decisions.
- Review workplace policies, privacy requirements, and data-retention rules before deployment.

## 🏁 Hackathon submission

- **Project:** Workplace Safety Sentinel
- **Participant ID:** CRA004
- **Repository:** `CraftifAIHackathon_CRA004`

This repository currently documents the experiment, configurations, and visual evidence. It does not yet contain a complete runnable PipeGen export, a verified Pipeline IR graph screenshot, or a validated restricted-zone safety pipeline. Add those artifacts only after they have actually been produced and checked.

---

<div align="center">

Built as an edge-AI safety monitoring prototype for the CraftifAI PipeGen Hackathon.

**Honest validation first. Reliable safety workflows next.**

</div>
