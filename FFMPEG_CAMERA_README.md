# FFmpeg Camera Implementation for Openterface

This implementation provides a multi-threaded FFmpeg-based camera capture system for Linux that integrates seamlessly with Qt's multimedia framework and QGraphicsVideoItem.

## Architecture Overview

The implementation consists of three main components:

### 1. FFmpegCameraThread (`ffmpegcamerathread.h/cpp`)
- **Purpose**: Low-level FFmpeg camera capture and decoding in a separate thread
- **Features**:
  - Multi-threaded frame capture using V4L2 and FFmpeg
  - Automatic device discovery and configuration
  - Frame rate control and performance monitoring
  - Hardware-accelerated decoding where available
  - Buffer management to prevent memory leaks

### 2. FFmpegIntegrateCameraManager (`ffmpegintegratecameramanager.h/cpp`)
- **Purpose**: Bridge between FFmpeg capture and Qt's multimedia framework
- **Features**:
  - Integration with QGraphicsVideoItem and QVideoWidget
  - Frame processing and conversion to QVideoFrame
  - Snapshot and area capture functionality
  - Performance monitoring and FPS reporting
  - Thread-safe frame queue management

### 3. Enhanced CameraManager (`cameramanager.h/cpp`)
- **Purpose**: Unified interface supporting both Qt and FFmpeg backends
- **Features**:
  - Backend switching between Qt Multimedia and FFmpeg
  - Automatic fallback from FFmpeg to Qt if needed
  - Preserved existing API compatibility
  - Seamless integration with existing Openterface code

## Key Benefits

1. **Performance**: Multi-threaded architecture prevents UI blocking
2. **Compatibility**: Works with existing QGraphicsVideoItem display system
3. **Reliability**: Automatic device detection and error recovery
4. **Flexibility**: Can switch between backends at runtime
5. **Linux Optimized**: Direct V4L2 integration for better hardware support

## Usage Example

```cpp
// Create camera manager (FFmpeg backend is default on Linux)
CameraManager* cameraManager = new CameraManager(this);

// Set up video display
QGraphicsVideoItem* videoItem = new QGraphicsVideoItem();
videoPane->setVideoItem(videoItem);

// Configure camera
cameraManager->setVideoOutput(videoItem);
cameraManager->setResolution(QSize(1920, 1080));
cameraManager->setFrameRate(30);

// Start camera with FFmpeg backend
cameraManager->setBackend(CameraManager::FFmpeg);
cameraManager->startCamera();

// The camera will automatically:
// 1. Detect the best available V4L2 device
// 2. Configure FFmpeg for optimal capture
// 3. Start multi-threaded frame decoding
// 4. Display frames in the QGraphicsVideoItem
```

## Backend Switching

The system supports runtime backend switching:

```cpp
// Switch to FFmpeg for better performance
cameraManager->setBackend(CameraManager::FFmpeg);

// Fallback to Qt Multimedia if needed
cameraManager->setBackend(CameraManager::QtMultimedia);
```

## Device Discovery

The system automatically discovers available cameras:

```cpp
// Get all available V4L2 devices
QStringList devices = FFmpegCameraThread::getAvailableV4L2Devices();

// Find the best Openterface device
QString openterfaceDevice = FFmpegCameraThread::findOpenterfaceDevice();

// Get supported resolutions for a device
QList<QSize> resolutions = FFmpegCameraThread::getSupportedResolutions("/dev/video0");
```

## Performance Monitoring

Real-time performance information is available:

```cpp
// Get current FPS
double fps = cameraManager->getCurrentFPS();

// Get dropped frame count
int dropped = cameraManager->getDroppedFrames();

// Get performance summary
QString perfInfo = ffmpegManager->getPerformanceInfo();
```

## Integration with Existing Code

The implementation maintains full compatibility with existing Openterface code:

- All existing CameraManager methods work unchanged
- VideoPane integration is seamless
- Image capture and recording functionality preserved
- Device switching and hotplug support maintained

## Build Requirements

Ensure these FFmpeg libraries are available:
- libavformat
- libavcodec 
- libavutil
- libswscale
- libswresample

The CMakeLists.txt has been updated to include the new source files.

## Technical Details

### Frame Processing Pipeline
1. V4L2 device captures raw frames
2. FFmpeg decodes frames in separate thread
3. Frames converted to RGB24 format
4. QVideoFrame created from RGB data
5. Frame sent to QVideoSink for display
6. QGraphicsVideoItem renders frame

### Thread Safety
- Frame queue with mutex protection
- Atomic variables for thread control
- Separate threads for capture and display
- Lock-free performance counters where possible

### Memory Management
- Automatic buffer cleanup
- Frame queue size limiting
- RAII patterns throughout
- Smart pointers for resource management

This implementation provides a robust, high-performance camera solution specifically optimized for Linux systems while maintaining full compatibility with the existing Openterface architecture.
