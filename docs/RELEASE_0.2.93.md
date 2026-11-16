# Brooklyn Subway 0.2.93

Improves feed downloads by reading available TLS data in blocks rather than one byte at a time.

- Gives each response body up to 12 seconds after headers arrive, with an 8-second idle timeout and a 30-second request limit checked during body reads.
- Continues rejecting incomplete, oversized and invalid feed bodies; certificate verification is unchanged.
- Adds serial body timing, byte counts and stop reasons for diagnosing interrupted downloads.
- Keeps existing Wi-Fi power-saving behavior and weather timeout limits.

Validation: host stream-reader and transport tests, all four firmware targets, and controlled COM6 USB tests. COM6 completed an 8/8 cycle, but a subsequent cycle was 7/8 after a body timeout. Intermittent receive stalls remain under investigation; this release does not promise uninterrupted feeds. COM5 was not flashed or reset during testing.
