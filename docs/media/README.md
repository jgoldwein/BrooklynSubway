# Working-map media

The project owner supplied the working-map video recorded November 17, 2026.
The public clip keeps the actual LED behavior, printed map and OLED contents.

- `brooklyn-subway-photo.jpg`: the selected conventional edit of the supplied
  photograph. A tight crop, mild perspective correction, small brightness/contrast
  adjustments and light sharpening preserve the original printed and electronic
  details. No text or OLED content was painted in. Location/EXIF metadata removed.
- `brooklyn-subway-demo.mp4`: 15.4 seconds, H.264, 1080 x 1050, 30 fps,
  silent, with fast-start metadata for browser playback.
- `brooklyn-subway-video-poster.jpg`: an actual frame at six seconds from the
  cropped public clip, not a generated rendering.

Processing: crop away the surrounding room; convert the original HLG HDR
footage to standard BT.709 color; apply a small exposure/contrast adjustment;
reduce 60 fps to 30 fps; remove audio and location/device metadata. No map
labels, components or lights were added or repainted. The original files are
kept outside the repository.

Tool: FFmpeg 7.1, provided by imageio-ffmpeg 0.6.0 (documentation/media tools
only; no firmware dependency).
