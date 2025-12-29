# USB UAC Audio Stops After 2 Seconds - Troubleshooting

## Current Status
Audio plays for ~2 seconds then stops completely. This suggests a clock synchronization or buffer management issue.

## Applied Fixes (Current Configuration)

### 1. Feedback Method
- **Method**: `AUDIO_FEEDBACK_METHOD_FIFO_COUNT` (proven, reliable)
- **Why**: Frequency-based methods require an audio master clock locked to sample rate, which this hardware doesn't have

### 2. Buffer Sizes  
- **MIC (IN)**: EP_SIZE × 3 (for HS mode)
- **Speaker (OUT)**: EP_SIZE × 3 (for HS mode)
- **Rationale**: Moderate increase from 2x (TinyUSB example) provides jitter tolerance without excessive latency

## Possible Causes of 2-Second Stoppage

### 1. Clock Drift
**Symptom**: FIFO gradually empties or fills until audio stops  
**Root cause**: Device sample rate doesn't exactly match host expectations  
**Solution**: Feedback should adjust this, but may not be working correctly

### 2. I2S Codec Issue
**Symptom**: ESP32 side stops consuming/producing audio data  
**Check**: Look for I2S underrun/overrun errors in logs  
**Solution**: Verify I2S clock configuration matches USB sample rate exactly

### 3. USB Stack Issue
**Symptom**: USB communication stops or resets  
**Check**: Monitor USB enumeration status  
**Solution**: Check USB cable, power supply, host compatibility

### 4. Buffer Size Mismatch
**Symptom**: With larger buffers, initial fill logic might malfunction  
**Check**: The `new_play` logic at startup  
**Solution**: May need to adjust SPK_NEW_PLAY_INTERVAL

## Debugging Steps

### 1. Enable Verbose Logging
Add to `sdkconfig`:
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE=y
```

Add to code:
```c
#define CFG_TUSB_DEBUG 2  // In tusb_config.h
```

### 2. Monitor FIFO Levels
Add logging in `tud_audio_rx_done_post_read_cb()`:
```c
int bytes_remained = tud_audio_available();
ESP_LOGI(TAG, "FIFO level: %d bytes", bytes_remained);
```

### 3. Check I2S Status
Add to `uac_device_output_cb()`:
```c
if (bytes_written != len) {
    ESP_LOGW(TAG, "I2S partial write: %d/%d", bytes_written, len);
}
```

### 4. Monitor Feedback Values
The feedback value should be close to the nominal rate. For 48kHz:
- **Nominal**: 48.000 (in 16.16 format: 0x000C0000)
- **Acceptable range**: 47.95 - 48.05

## Quick Tests

### Test 1: Revert to Original Buffer Sizes
If this was working before (with occasional jitter), try:
```c
// In tusb_config_uac.h, remove the #if CONFIG_TINYUSB_RHPORT_HS blocks
// Use the FS path for both, which uses (SPK_INTERVAL_MS + 1)
```

### Test 2: Try Different Sample Rates
Sometimes clock issues are worse at certain rates:
- Try 44.1kHz instead of 48kHz
- Try 16kHz (lower bandwidth)

### Test 3: Disable Feedback
Temporarily force a fixed feedback value:
```c
// In usb_device_uac.c, add after feedback_param setup:
tud_audio_n_fb_set(0, 0x000C0000);  // Fixed 48kHz feedback
```

### Test 4: Check Host OS
Different OSes have different USB audio stack behaviors:
- **Windows**: Often requires 16.16 format even for FS
- **macOS**: Strict about timing
- **Linux**: Most flexible

## Expected Log Output

```
I (123) usbd_uac: Feedback: FIFO count method (optimized), sample_freq=48000 Hz
I (234) usbd_uac: Speaker interface 1-1 opened
I (345) usbd_uac: FIFO level: 196 bytes  // Should stay around this
I (456) usbd_uac: FIFO level: 392 bytes
I (567) usbd_uac: FIFO level: 196 bytes
... (should keep cycling)
```

## If Audio Still Stops

### Option 1: Increase Buffers More Aggressively
Try EP_SIZE × 6 or even × 8 to see if it's purely a buffer size issue.

### Option 2: Add Feedback Debugging
Log the actual feedback values being sent to the host.

### Option 3: Check for Memory Issues
USB audio uses DMA - ensure buffers are in DMA-capable memory.

### Option 4: Try External I2S MCLK
If available, use an external audio master clock for more stable timing.

## Configuration Summary

Current `tusb_config_uac.h` settings:
```c
#if CONFIG_TINYUSB_RHPORT_HS
  MIC:  EP_SZ × 3    // ~588 bytes @ 48kHz stereo
  SPK:  EP_SZ × 3    // ~588 bytes @ 48kHz stereo  
#else
  MIC:  EP_SZ × (MIC_INTERVAL_MS + 1)
  SPK:  EP_SZ × (SPK_INTERVAL_MS + 1)    // = × 11
#endif
```

Current `usb_device_uac.c` settings:
```c
Feedback: AUDIO_FEEDBACK_METHOD_FIFO_COUNT
Sample rate: 48000 Hz (CONFIG_UAC_SAMPLE_RATE)
```

## Next Steps

1. **Flash and test** current configuration
2. **Monitor serial output** for clues
3. **If still fails**: Try reverting to original buffer sizes (remove HS-specific sizing)
4. **If still fails**: Test with different host OS or USB cable

The most likely issue is that the larger buffers are confusing the startup buffering logic (`new_play`), or the FIFO_COUNT feedback isn't adapting quickly enough to the buffer size change.

