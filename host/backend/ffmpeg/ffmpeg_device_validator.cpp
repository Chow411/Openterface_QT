/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
* ========================================================================== *
*/

#include "ffmpeg_device_validator.h"
#include "global.h"
#include "ui/globalsetting.h"

#include <QFile>
#include <QDebug>
#include <QLoggingCategory>
#include <QSet>

extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <dshow.h>
#include <strmif.h>
// COM smart pointers for DirectShow
#include <comdef.h>
#endif

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#endif

Q_DECLARE_LOGGING_CATEGORY(log_ffmpeg_backend)

FFmpegDeviceValidator::FFmpegDeviceValidator()
{
}

FFmpegDeviceValidator::~FFmpegDeviceValidator()
{
}

bool FFmpegDeviceValidator::CheckCameraAvailable(const QString& devicePath, 
                                                  const QString& currentDevice,
                                                  bool captureRunning,
                                                  bool waitingForDevice)
{
    if (devicePath.isEmpty()) {
        qCDebug(log_ffmpeg_backend) << "No device path provided for availability check";
        return false;
    }
    
    qCDebug(log_ffmpeg_backend) << "Checking camera availability for device:" << devicePath;
    
    // OS-specific device access check
    if (!CheckOSSpecificDeviceAccess(devicePath, currentDevice, captureRunning)) {
        return false;
    }
    
    // Skip intrusive FFmpeg check if device is currently being used for capture
    if (devicePath == currentDevice && captureRunning) {
        qCDebug(log_ffmpeg_backend) << "Device is currently in use for capture, skipping FFmpeg compatibility check";
        return true;
    }
    
    // Skip intrusive FFmpeg check if we're waiting for device activation
    if (waitingForDevice) {
        qCDebug(log_ffmpeg_backend) << "Waiting for device activation, skipping intrusive FFmpeg compatibility check";
        return true; // Rely on OS-specific checks above
    }
    
    // FFmpeg compatibility check
    return CheckFFmpegCompatibility(devicePath);
}

bool FFmpegDeviceValidator::GetMaxCameraCapability(const QString& devicePath, CameraCapability& capability)
{
    qCInfo(log_ffmpeg_backend) << "Loading video settings from GlobalSetting for:" << devicePath;
    
    // Load video settings from GlobalSetting into GlobalVar
    GlobalSetting::instance().loadVideoSettings();
    
    // Get the stored resolution and framerate
    int width = GlobalVar::instance().getCaptureWidth();
    int height = GlobalVar::instance().getCaptureHeight();
    int fps = GlobalVar::instance().getCaptureFps();
    
    capability.resolution = QSize(width, height);
    capability.framerate = fps;
    
    qCInfo(log_ffmpeg_backend) << "✓ Maximum capability from GlobalSetting:" 
                              << capability.resolution << "@" << capability.framerate << "FPS";
    return true;
}

bool FFmpegDeviceValidator::CheckOSSpecificDeviceAccess(const QString& devicePath, 
                                                        const QString& currentDevice,
                                                        bool captureRunning)
{
#ifdef Q_OS_WIN
    // On Windows, DirectShow device names like "video=Openterface" are not file paths
    // Skip file existence check for DirectShow devices
    if (devicePath.startsWith("video=")) {
        qCDebug(log_ffmpeg_backend) << "DirectShow device detected, skipping file existence check:" << devicePath;
        return true;
    }
    
    // For V4L2 devices on Windows (unlikely but handle it)
    QFile deviceFile(devicePath);
    if (!deviceFile.exists()) {
        qCDebug(log_ffmpeg_backend) << "Device file does not exist:" << devicePath;
        return false;
    }
    
    // Try to open the device for reading to verify it's accessible
    // Skip this check if we're currently capturing to avoid device conflicts
    if (devicePath == currentDevice && captureRunning) {
        qCDebug(log_ffmpeg_backend) << "Device is currently in use for capture, skipping file open check";
        return true;
    }
    
    if (!deviceFile.open(QIODevice::ReadOnly)) {
        qCDebug(log_ffmpeg_backend) << "Cannot open device for reading:" << devicePath << "Error:" << deviceFile.errorString();
        return false;
    }
    
    deviceFile.close();
#else
    // On Linux/macOS, check if device file exists and is accessible
    QFile deviceFile(devicePath);
    if (!deviceFile.exists()) {
        qCDebug(log_ffmpeg_backend) << "Device file does not exist:" << devicePath;
        return false;
    }
    
    // Try to open the device for reading to verify it's accessible
    // Skip this check if we're currently capturing to avoid device conflicts
    if (devicePath == currentDevice && captureRunning) {
        qCDebug(log_ffmpeg_backend) << "Device is currently in use for capture, skipping file open check";
        return true;
    }
    
    if (!deviceFile.open(QIODevice::ReadOnly)) {
        qCDebug(log_ffmpeg_backend) << "Cannot open device for reading:" << devicePath << "Error:" << deviceFile.errorString();
        return false;
    }
    
    deviceFile.close();
#endif
    
    return true;
}

bool FFmpegDeviceValidator::CheckFFmpegCompatibility(const QString& devicePath)
{
    AVFormatContext* testContext = avformat_alloc_context();
    if (!testContext) {
        qCDebug(log_ffmpeg_backend) << "Failed to allocate test format context";
        return false;
    }
    
    const AVInputFormat* inputFormat = GetInputFormat();
    if (!inputFormat) {
        avformat_free_context(testContext);
        return false;
    }
    
    // Try to open the device with minimal options
    AVDictionary* options = nullptr;
    av_dict_set(&options, "framerate", "1", 0); // Very low framerate for quick test
    av_dict_set(&options, "video_size", "160x120", 0); // Very small resolution for quick test
    
    int ret = avformat_open_input(&testContext, devicePath.toUtf8().constData(), inputFormat, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        qCDebug(log_ffmpeg_backend) << "FFmpeg cannot open device:" << devicePath << "Error:" << QString::fromUtf8(errbuf);
        avformat_free_context(testContext);
        return false;
    }
    
    // Device opened successfully, clean up
    avformat_close_input(&testContext);
    qCDebug(log_ffmpeg_backend) << "Camera device is available:" << devicePath;
    return true;
}

const AVInputFormat* FFmpegDeviceValidator::GetInputFormat()
{
#ifdef Q_OS_WIN
    // On Windows, use DirectShow input format
    avdevice_register_all();
    const AVInputFormat* inputFormat = av_find_input_format("dshow");
    if (!inputFormat) {
        qCDebug(log_ffmpeg_backend) << "DirectShow input format not available";
        return nullptr;
    }
    return inputFormat;
#else
    // On Linux/macOS, use V4L2 input format
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    if (!inputFormat) {
        qCDebug(log_ffmpeg_backend) << "V4L2 input format not available";
        return nullptr;
    }
    return inputFormat;
#endif
}

// Default resolutions for Openterface KVM cameras (MS2109/MS2130 chips)
static QList<QSize> GetDefaultResolutions()
{
    return {
        QSize(1920, 1080),
        QSize(1280, 720),
        QSize(1024, 768),
        QSize(800, 600),
        QSize(640, 480),
        QSize(320, 240)
    };
}

#ifdef Q_OS_WIN
// Helper to free AM_MEDIA_TYPE structure (equivalent to DeleteMediaType from DirectShow base classes)
static void FreeMediaType(AM_MEDIA_TYPE& mt)
{
    if (mt.cbFormat != 0) {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
    }
    if (mt.pUnk != nullptr) {
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}

// Helper to enumerate DirectShow filter formats via IAMStreamConfig
static bool EnumerateDirectShowResolutions(const QString& deviceName, QSet<QSize>& outResolutions)
{
    ICreateDevEnum* pDevEnum = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, (void**)&pDevEnum);
    if (FAILED(hr)) return false;

    IEnumMoniker* pEnumMoniker = nullptr;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMoniker, 0);
    if (FAILED(hr) || !pEnumMoniker) {
        pDevEnum->Release();
        return false;
    }

    IMoniker* pMoniker = nullptr;
    ULONG cFetched;
    bool found = false;

    while (!found && pEnumMoniker->Next(1, &pMoniker, &cFetched) == S_OK) {
        IPropertyBag* pPropBag = nullptr;
        hr = pMoniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void**)&pPropBag);
        if (SUCCEEDED(hr)) {
            VARIANT varName;
            VariantInit(&varName);
            hr = pPropBag->Read(L"FriendlyName", &varName, nullptr);
            if (SUCCEEDED(hr) && varName.bstrVal) {
                QString friendlyName = QString::fromWCharArray(varName.bstrVal);
                VariantClear(&varName);

                if (friendlyName.contains(deviceName, Qt::CaseInsensitive)) {
                    qCDebug(log_ffmpeg_backend) << "DirectShow: Found matching device:" << friendlyName;

                    IBaseFilter* pFilter = nullptr;
                    hr = pMoniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&pFilter);
                    if (SUCCEEDED(hr)) {
                        IEnumPins* pEnumPins = nullptr;
                        hr = pFilter->EnumPins(&pEnumPins);
                        if (SUCCEEDED(hr)) {
                            IPin* pPin = nullptr;
                            while (pEnumPins->Next(1, &pPin, &cFetched) == S_OK) {
                                PIN_DIRECTION dir;
                                pPin->QueryDirection(&dir);
                                if (dir == PINDIR_OUTPUT) {
                                    IAMStreamConfig* pConfig = nullptr;
                                    hr = pPin->QueryInterface(IID_IAMStreamConfig, (void**)&pConfig);
                                    if (SUCCEEDED(hr)) {
                                        int iCount = 0, iSize = 0;
                                        hr = pConfig->GetNumberOfCapabilities(&iCount, &iSize);
                                        if (SUCCEEDED(hr) && iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
                                            for (int i = 0; i < iCount; i++) {
                                                VIDEO_STREAM_CONFIG_CAPS scc;
                                                AM_MEDIA_TYPE* pmt = nullptr;
                                                hr = pConfig->GetStreamCaps(i, &pmt, (BYTE*)&scc);
                                                if (SUCCEEDED(hr) && pmt && pmt->formattype == FORMAT_VideoInfo) {
                                                    if (pmt->cbFormat >= sizeof(VIDEOINFOHEADER)) {
                                                        VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)pmt->pbFormat;
                                                        QSize res(pVIH->bmiHeader.biWidth, abs(pVIH->bmiHeader.biHeight));
                                                        if (res.width() > 0 && res.height() > 0) {
                                                            outResolutions.insert(res);
                                                        }
                                                    }
                                                    FreeMediaType(*pmt);
                                                    CoTaskMemFree(pmt);
                                                }
                                            }
                                        }
                                        pConfig->Release();
                                    }
                                }
                                pPin->Release();
                            }
                            pEnumPins->Release();
                        }
                        pFilter->Release();
                        found = !outResolutions.isEmpty();
                    }
                }
            }
            pPropBag->Release();
        }
        pMoniker->Release();
    }
    pEnumMoniker->Release();
    pDevEnum->Release();
    return found;
}
#endif

QList<QSize> FFmpegDeviceValidator::GetSupportedResolutions(const QString& deviceName)
{
    QSet<QSize> resolutionSet;

#ifdef Q_OS_WIN
    // Windows: Use DirectShow IAMStreamConfig to enumerate supported formats
    qCInfo(log_ffmpeg_backend) << "Enumerating camera resolutions via DirectShow for device:" << deviceName;

    // Ensure COM is initialized for this thread
    bool needCoUninit = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        needCoUninit = true;
    } else if (hr == RPC_E_CHANGED_MODE) {
        // Already initialized in a different mode, that's OK
    }

    if (EnumerateDirectShowResolutions(deviceName, resolutionSet) && !resolutionSet.isEmpty()) {
        QList<QSize> result = resolutionSet.values();
        std::sort(result.begin(), result.end(), [](const QSize& a, const QSize& b) {
            return a.width() * a.height() > b.width() * b.height();
        });
        qCInfo(log_ffmpeg_backend) << "DirectShow found" << result.size() << "resolutions";
        if (needCoUninit) CoUninitialize();
        return result;
    }

    if (needCoUninit) CoUninitialize();
    qCWarning(log_ffmpeg_backend) << "DirectShow enumeration failed or found no resolutions, using defaults";

#elif defined(Q_OS_LINUX)
    // Linux: Use V4L2 VIDIOC_ENUM_FRMSIZE to enumerate supported resolutions
    qCInfo(log_ffmpeg_backend) << "Enumerating camera resolutions via V4L2 for device:" << deviceName;

    QString devicePath = deviceName;
    if (devicePath.isEmpty() || !devicePath.startsWith("/dev/")) {
        devicePath = "/dev/video0";
    }

    int fd = open(devicePath.toUtf8().constData(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
        struct v4l2_frmsizeenum frmsize;
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.index = 0;
        frmsize.pixel_format = V4L2_PIX_FMT_MJPEG;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                QSize res(frmsize.discrete.width, frmsize.discrete.height);
                if (res.width() > 0 && res.height() > 0) {
                    resolutionSet.insert(res);
                }
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE || frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                // Add common resolutions within the stepwise range
                for (int w = frmsize.stepwise.min_width; w <= (int)frmsize.stepwise.max_width; w += 160) {
                    for (int h = frmsize.stepwise.min_height; h <= (int)frmsize.stepwise.max_height; h += 120) {
                        if (w % 16 == 0 && h % 8 == 0) { // Alignment requirement
                            resolutionSet.insert(QSize(w, h));
                        }
                    }
                }
                break;
            }
            frmsize.index++;
        }
        close(fd);

        if (!resolutionSet.isEmpty()) {
            QList<QSize> result = resolutionSet.values();
            std::sort(result.begin(), result.end(), [](const QSize& a, const QSize& b) {
                return a.width() * a.height() > b.width() * b.height();
            });
            qCInfo(log_ffmpeg_backend) << "V4L2 found" << result.size() << "resolutions";
            return result;
        }
    }
    qCWarning(log_ffmpeg_backend) << "V4L2 enumeration failed, using default resolutions";
#else
    Q_UNUSED(deviceName);
#endif

    // Fallback to default resolutions
    QList<QSize> defaults = GetDefaultResolutions();
    qCInfo(log_ffmpeg_backend) << "Using default resolutions:" << defaults.size() << "items";
    return defaults;
}
