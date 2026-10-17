# AirGradient ONE with direct Apple Home support

Custom firmware for the **Wi-Fi AirGradient ONE (I-9PSL / PCB v9)**. Adds HomeSpan
without replacing AirGradient's sensor drivers, OLED screens, LED bar, calibration,
Wi-Fi provisioning, MQTT, or local HTTP API. No Homebridge or Home Assistant server
is required. An Apple TV or HomePod home hub is recommended for reliable operation,
remote access and automations.

This is an experimental build. It has been installed on a ONE and checked for
sensor updates and HomeKit discovery. Apple Home pairing and long-run stability
still require verification; see the validation checklist below. Do not flash an
older ESP8266 DIY Pro, Open Air, or cellular-only monitor with this build.

## Build and flash

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
(or the PlatformIO VS Code extension), then from this directory:

```sh
pio run -e one-homekit
```

The build pins ESP32 platform 6.10.0 (Arduino core 2.0.17) and HomeSpan 1.9.1.
AirGradient currently requires core 2.x; HomeSpan 2.x requires core 3.x. Do not
upgrade either independently. The upstream submodules are vendored, so no recursive
clone or external AirGradient library install is needed.

1. Connect the ONE using a USB-C **data** cable. Stop any other serial monitor.
2. Find its port with `pio device list` (typically `/dev/cu.usbmodem...` on macOS).
3. Upload with `pio run -e one-homekit -t upload --upload-port /dev/cu.usbmodemXXXX`.
4. Open the serial monitor with
   `pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200`.
5. Provision Wi-Fi using the normal AirGradient setup, if needed. Keep the device
   in online / Wi-Fi mode: AirGradient's **offline mode disables Wi-Fi**. Disabling
   its cloud connection is fine; HomeKit operates locally.
6. Restart while the monitor is open and record `Apple Home setup code: XXX-XX-XXX`.
   The random device-specific code is stored in NVS and printed at each boot.
7. In Apple Home, choose **Add Accessory → More Options**, select
   `AirGradient ONE <last six serial digits>`, accept the uncertified accessory
   prompt, and enter the setup code.

The firmware does not take control of the LED bar or button for HomeSpan status.
The setup code is available over USB serial, so existing OLED content is preserved.
Keep your phone and monitor on a LAN that allows mDNS and local connections.

If upload cannot connect, hold BOOT while resetting/powering up the ONE, then retry.
Flashing normally preserves configuration and HomeKit pairing. Before initial
flashing, record your existing settings. A full flash erase removes all settings
and pairings; only use it when deliberately starting fresh.

## What Apple Home receives

| Service | Values |
| --- | --- |
| Temperature | Celsius internally, using AirGradient's configured correction |
| Humidity | Relative humidity, using configured correction |
| Carbon dioxide | ppm and high-CO₂ indication at ≥1000 ppm |
| Air quality | PM2.5 and PM10 in µg/m³, plus a five-level rating based on PM2.5 |
| VOC Air Quality | Five-level rating derived from relative VOC Index |
| NOx Air Quality | Five-level rating derived from relative NOx Index |

PM2.5 uses AirGradient's configured correction and moving average. HomeKit values
refresh approximately every five seconds after upstream sensor sampling. Home's
presentation of individual values depends on the Apple app/version; Apple Home may
group the air-quality services under this accessory. The PM-based rating is unchanged,
using PM2.5 cutoffs 9, 35.4, 55.4, and 125.4 µg/m³.

### VOC and NOx indices

Two named, standard AirQualitySensor services provide qualitative gas-activity ratings
in Apple Home. Their custom, read-only numeric characteristics expose **VOC Index
(unitless)** and **NOx Index (unitless)** to apps such as Eve for HomeKit. These are
the upstream moving-average index values, with no ppm/ppb/µg/m³ conversion or unit.
Apple Home generally ignores the numeric custom characteristics. Actual presentation
in Eve and Apple Home must be checked on the user's phone.

| HomeKit rating | VOC Index | NOx Index |
| --- | --- | --- |
| Excellent | 1–100 | 1 |
| Good | >100–150 | >1–20 |
| Fair | >150–250 | >20–150 |
| Inferior | >250–400 | >150–300 |
| Poor | >400–500 | >300–500 |

These describe **relative activity**, not absolute exposure or health limits. They
follow [AirGradient's dashboard index bands](https://www.airgradient.com/documentation/kb/airgradient-dashboard-measurement-and-interpretation),
with the green band split at the [Sensirion baseline](https://sensirion.com/resource/datasheet/sgp41)
(VOC 100, NOx 1) to use HomeKit's five labels. A low index does not prove clean air:
the algorithm adapts to its surroundings.

Zero is the SGP41 algorithm's warm-up/unavailable output. Zero, out-of-range,
non-finite and stale readings produce **Unknown** and a service fault. The numeric
characteristic retains its last valid value (or starts at 0 with a fault) until a
valid index in 1–500 is available. VOC/NOx faults do not affect the PM service.

The gas services are appended to the existing accessory database so all pre-existing
service/characteristic IDs and stored pairing data remain stable. HomeKit detects
the database change on reconnect; try closing and reopening Home/Eve if the new
services do not immediately appear. There is no need to erase flash or reset pairing.

Missing/invalid readings set the service's `StatusFault`; the last valid numeric
value is retained. Air quality becomes Unknown when PM2.5 is invalid. A stalled
sensor-loop snapshot expires after 30 seconds. Upstream moving averages may retain
readings through brief sensor dropouts before declaring them invalid.

HomeKit uses TCP **1201**, leaving HTTP on **80**. This build uses
`airgradient-<serial>.local` (hyphen), rather than upstream's underscore hostname,
because HomeSpan requires a valid DNS hostname. Both `_hap._tcp` and
`_airgradient._tcp` advertise the same device. Existing clients using the old
hostname need updating, or can use the device's IP.

## Updates and reset

Stock AirGradient OTA is disabled in the HomeKit build: installing a stock OTA
would remove HomeKit. The HomeKit build uses a single larger application slot, keeping NVS and SPIFFS
at their original offsets. Update this custom firmware over USB. HomeSpan OTA and serial
commands are disabled, and Wi-Fi setup remains AirGradient's normal workflow.

Remove the accessory in Apple Home before deliberately clearing pairing. The
existing AirGradient button factory-reset gesture also clears HomeSpan pairing,
its cached Wi-Fi credentials, and the setup code. After restart, provision again
and use the newly generated code. Pairings survive ordinary power cycles/uploads.
To restore stock firmware, use AirGradient's official USB web flasher; erase flash
if you want all custom configuration removed.

## Validation

Host-side boundary tests (requires a C++ compiler):

```sh
c++ -std=c++11 -Wall -Wextra -Werror tests/homekit_policy_test.cpp -o /tmp/ag-homekit-test
/tmp/ag-homekit-test
```

Build verified locally on 17 October 2026 with PlatformIO 6.2.0. The HomeKit target
compiles successfully. The final binary size is checked against the application
partition, including alignment padding: 2,002,784 / 3,932,160 bytes (50.9%).
Static RAM use is 74,324 bytes (22.7%). Runtime heap and pairing are not measured by these figures.
The host boundary tests pass. Compiled application and checksum metadata are in
`build/homekit/`; this is an application image, not a merged factory image.

Installed and checked on ONE `84fce612f044` on 17 October 2026: flash writes were
hash-verified; HomeSpan database validation reported zero warnings/errors;
`_hap._tcp` resolved on port 1201; an unauthenticated HomeKit request correctly
returned HTTP 470; the local API returned live temperature, humidity, CO2 and PM
readings with the configured CO2 LED mode retained. No assertion or reboot was
observed over the initial 90-second serial observation. This does not establish
long-run reliability or confirm the physical appearance of the display/LEDs.

The VOC/NOx update (`3.7.0-homekit.2`) was also installed and checked on this device:
the two stored paired controllers were retained; all 17 existing characteristic
IDs/UUIDs matched the prior build; database validation reported zero warnings/errors;
the live gas update path logged VOC 22.17 / NOx 1.00, both mapped to rating 1.
Apple Home/Eve presentation and automation behavior have not been verified on a phone.

Further physical-device acceptance checks:

- Provision, pair with Apple Home, and confirm temperature, humidity, CO₂ and PM.
- Compare readings with AirGradient's local API; check OLED and LED modes/brightness.
- Reboot and confirm pairing survives; interrupt Wi-Fi and confirm recovery.
- Exercise calibration and display animations while watching HomeKit responsiveness.
- Disconnect a sensor and confirm a fault rather than a fabricated zero reading.
- Factory-reset, reprovision, and pair again using the new setup code.
- Run for at least 24 hours, checking free heap, watchdog resets, and “No Response”.

## Source and licensing

Based on [airgradienthq/arduino](https://github.com/airgradienthq/arduino), version
3.7.0, commit `1d2911b110e2f1319ea18db6ef7d71ec15cf0e88`.
Vendored dependencies: airgradient-client `1b5919e0d8f1feedd9f1f8063304bd16b2b3d11d`
and airgradient-ota `8656a6fdbcad3eb996858b8db1e36da094c42f66`.
[HomeSpan](https://github.com/HomeSpan/HomeSpan/tree/1.9.1) is MIT licensed and fetched
by PlatformIO. AirGradient firmware and modifications follow the existing
[CC BY-SA 4.0 license](LICENSE). Bundled libraries retain their own notices.
See [README.upstream.md](README.upstream.md) for the original project introduction.
