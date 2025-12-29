# USB UAC Audio Device for ESP32-P4

A high-speed USB Audio Class (UAC) 2.0 device implementation for ESP32-P4, enabling USB audio streaming with bi-directional audio support (speaker output and microphone input).

## 🎵 Overview

This project implements a USB Audio Device that allows the ESP32-P4 to function as a USB sound card when connected to a host computer (Windows, macOS, or Linux). It supports both audio playback (speaker) and recording (microphone) with configurable channels and sample rates.

### Key Features

- **USB Audio Class 2.0 compliance** - Works as a standard USB audio device
- **High-Speed USB (480 Mbps)** - Low latency audio streaming
- **Bi-directional audio** - Simultaneous playback and recording
- **Multi-channel support**:
  - Up to 8 speaker channels (default: 2 stereo)
  - Up to 4 microphone channels (default: 2 stereo)
- **Configurable sample rates** - Default 48 kHz, supports 16/32/44.1/48 kHz
- **USB Audio controls** - Volume adjustment and mute control from host
- **Asynchronous feedback endpoint** - Adaptive clock synchronization
- **I2S audio codec integration** - Professional audio quality

## 🛠️ Hardware Requirements

- **ESP32-P4** microcontroller (with USB OTG HS support)
- **Audio codec** with I2S interface (e.g., CS4344, PCM5102, WM8978)
- **USB connection** to host computer
- **Power supply** adequate for USB HS operation

### Tested Configuration

- MCU: ESP32-P4
- Audio codec: Custom I2S-based codec
- Sample rate: 48 kHz
- Bit depth: 16-bit
- Channels: 2 stereo (speaker + microphone)

## 📋 Prerequisites

- **ESP-IDF v5.5+** - Espressif IoT Development Framework
- **CMake 3.13+**
- **Python 3.7+**
- **USB cable** with data lines (not charge-only)

## 🚀 Quick Start

### 1. Clone and Setup

```bash
# Clone this repository
git clone <your-repo-url>
cd usb_uac

# Setup ESP-IDF environment
. $IDF_PATH/export.sh
```

### 2. Configure

```bash
# Configure project settings
idf.py menuconfig
```

Key configuration options:
- **Component config → USB Device UAC**
  - Sample rate: 48000 Hz (default)
  - Speaker channels: 2
  - Microphone channels: 2
  - Speaker interval: 10 ms
  - Microphone interval: 1 ms

- **Component config → TinyUSB Stack**
  - Enable high-speed mode: Yes (CONFIG_TINYUSB_RHPORT_HS)

### 3. Build and Flash

```bash
# Build the project
idf.py build

# Flash to ESP32-P4
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor
```

### 4. Connect to Host

1. Connect ESP32-P4 USB port to your computer
2. The device should enumerate as "ESP USB Audio Device"
3. Select it as your audio input/output device in system settings

**Windows**: Settings → Sound → Choose audio device  
**macOS**: System Preferences → Sound → Select USB Audio  
**Linux**: Settings → Sound → Choose USB Audio Device

## 📂 Project Structure

```
usb_uac/
├── main/
│   ├── usb_uac_main.c          # Main application entry point
│   ├── codec.c/h               # I2S audio codec driver
│   └── CMakeLists.txt
├── components/
│   ├── usb_device_uac.c        # USB UAC device implementation
│   ├── include/
│   │   └── usb_device_uac.h   # Public API
│   ├── tusb/                   # TinyUSB port files
│   └── tusb_uac/               # UAC-specific descriptors & config
│       ├── uac_descriptors.h   # USB descriptors
│       ├── uac_config.h        # UAC channel/rate config
│       └── tusb_config_uac.h   # TinyUSB UAC configuration
├── managed_components/
│   └── espressif__tinyusb/     # TinyUSB library (ESP-IDF component)
├── CMakeLists.txt
├── sdkconfig.defaults          # Default project configuration
└── README.md
```

## 🔧 Configuration

### Audio Settings

Edit `sdkconfig.defaults` or use menuconfig:

```ini
# Sample rate (Hz)
CONFIG_UAC_SAMPLE_RATE=48000

# Number of channels
CONFIG_UAC_SPEAKER_CHANNEL_NUM=2
CONFIG_UAC_MIC_CHANNEL_NUM=2

# Transfer intervals (ms)
CONFIG_UAC_SPK_INTERVAL_MS=10
CONFIG_UAC_MIC_INTERVAL_MS=1
```

### Custom Codec Integration

Implement these functions in `main/codec.c`:

```c
void InitCodec();                                    // Initialize I2S codec
void i2s_write(void *buf, uint32_t size, ...);      // Write audio data
void i2s_read(void *buf, uint32_t size, ...);       // Read audio data
void SetMute(uint32_t mute_l, uint32_t mute_r);     // Mute control
void SetOutputLevels(uint32_t left, uint32_t right);// Volume control
```

## 🎛️ API Usage

### Initialize USB Audio Device

```c
#include "usb_device_uac.h"

// Define callbacks
static esp_err_t uac_output_cb(uint8_t *buf, size_t len, void *arg) {
    // Write audio data to I2S/codec
    i2s_write(buf, len, &bytes_written);
    return ESP_OK;
}

static esp_err_t uac_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg) {
    // Read audio data from I2S/codec
    i2s_read(buf, len, bytes_read);
    return ESP_OK;
}

// Initialize device
uac_device_config_t config = {
    .output_cb = uac_output_cb,      // Speaker data callback
    .input_cb = uac_input_cb,        // Microphone data callback
    .set_mute_cb = uac_set_mute_cb,  // Optional: mute control
    .set_volume_cb = uac_set_vol_cb, // Optional: volume control
    .cb_ctx = NULL,
};

esp_err_t ret = uac_device_init(&config);
```

## 🐛 Troubleshooting

### Device Not Recognized

- **Check USB cable** - Must support data, not just charging
- **Verify USB mode** - Ensure high-speed mode is enabled in config
- **Check logs** - Look for enumeration messages in `idf.py monitor`

### Audio Stuttering/Glitches

- **Clock jitter** - Normal for USB audio without external MCLK
- **Lower sample rate** - Try 16 kHz or 32 kHz for better stability
- **Check FIFO levels** - Monitor logs for buffer underrun/overflow warnings
- **USB bandwidth** - Disconnect other USB devices to reduce bus load

### No Audio Output

- **I2S configuration** - Verify codec is properly initialized
- **Volume/mute** - Check volume isn't at 0 or device isn't muted
- **Sample rate mismatch** - Ensure I2S matches USB configuration (48 kHz)
- **Feedback endpoint** - Check logs for feedback configuration

### Known Limitations

- **Dynamic sample rate switching** - Not supported (fixed at compile time)
- **macOS compatibility** - Requires `CONFIG_UAC_SUPPORT_MACOS=y`
- **Occasional sync loss** - Can occur due to clock drift (hardware limitation)

## 📊 Performance

- **Latency**: ~20-30ms (depends on buffer size and interval settings)
- **Bit depth**: 16-bit (configurable to 24-bit)
- **Sample rates**: 16/32/44.1/48 kHz
- **USB bandwidth**: ~1.5 Mbps @ 48kHz stereo (well within HS 480 Mbps)
- **CPU usage**: ~15-20% on ESP32-P4 @ 360 MHz

## 🔬 Technical Details

### USB Descriptors

- **Device Class**: Audio (0x01)
- **Audio Class**: UAC 2.0
- **Endpoints**:
  - Control endpoint (EP0)
  - Isochronous OUT (speaker data)
  - Isochronous IN (microphone data)
  - Isochronous feedback (clock synchronization)

### Clock Synchronization

Uses **asynchronous feedback endpoint** with FIFO-based rate adaptation:
- Monitors buffer fill level
- Adjusts feedback value to host
- Compensates for clock differences between USB host and I2S codec

### Buffer Management

- **Speaker buffer**: 11 × frame size (~2.1 KB @ 48 kHz stereo)
- **Microphone buffer**: 11 × frame size
- **Transfer interval**: Configurable (1-10 ms typical)

## 📚 Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [USB Audio Class 2.0 Specification](https://www.usb.org/document-library/audio-device-class-20)
- [ESP32-P4 Technical Reference](https://www.espressif.com/en/products/socs/esp32-p4)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## 📄 License

- **Application code**: Unlicense / CC0-1.0 (public domain)
- **CTAG TBD codec driver**: GPL 3.0 (see `main/codec.h`)
- **ESP-IDF components**: Apache 2.0
- **TinyUSB library**: MIT License

See individual file headers for specific licensing information.

## 🙏 Acknowledgments

- **Espressif Systems** - ESP-IDF framework and USB UAC component
- **TinyUSB** - Excellent USB device stack
- **CTAG TBD Project** - Audio codec driver foundation
- **Creative Technologies Arbeitsgruppe** - Kiel University of Applied Sciences

## 📧 Support

For issues and questions:
- Open an issue on GitHub
- Check ESP-IDF forums
- Review TinyUSB documentation

---

**Built with ❤️ for the ESP32-P4 community**

