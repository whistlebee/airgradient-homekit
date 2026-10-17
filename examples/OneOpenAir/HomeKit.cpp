#ifdef AG_HOMEKIT
#include "HomeKit.h"
#include "HomeKitPolicy.h"
#include <HomeSpan.h>
#include <Preferences.h>
#include <cmath>
#include "esp_system.h"

// Stable, project-specific UUIDs. No concentration units: these are relative indices.
CUSTOM_CHAR(VocIndex, E59DC431-0717-4E41-B762-1FD0A56BFD6A, PR+EV, FLOAT, 0, 0, 500, true);
CUSTOM_CHAR(NoxIndex, 93907810-0F42-4CAE-AF62-24EBD42C8B9D, PR+EV, FLOAT, 0, 0, 500, true);

namespace homekit {
namespace {
struct Reading {
  float temperature = NAN, humidity = NAN, co2 = NAN, pm25 = NAN, pm10 = NAN;
  float voc = NAN, nox = NAN;
  uint32_t at = 0;
  bool available = false;
};
Reading latest;
portMUX_TYPE readingMux = portMUX_INITIALIZER_UNLOCKED;
bool started = false;

struct Sensors : Service::TemperatureSensor {
  SpanCharacteristic *temperature, *temperatureFault, *humidity, *humidityFault;
  SpanCharacteristic *co2, *co2Detected, *co2Fault, *pm25, *pm10, *quality, *pmFault;
  SpanCharacteristic *vocIndex, *vocQuality, *vocFault, *noxIndex, *noxQuality, *noxFault;
  uint32_t lastUpdate = 0;
  bool gasReadingsLogged = false;

  Sensors() {
    temperature = new Characteristic::CurrentTemperature(0);
    temperature->setRange(-40, 125);
    temperatureFault = new Characteristic::StatusFault(1);
    new Service::HumiditySensor();
    humidity = new Characteristic::CurrentRelativeHumidity(0);
    humidityFault = new Characteristic::StatusFault(1);
    new Service::CarbonDioxideSensor();
    co2Detected = new Characteristic::CarbonDioxideDetected(0);
    co2 = new Characteristic::CarbonDioxideLevel(0);
    co2Fault = new Characteristic::StatusFault(1);
    new Service::AirQualitySensor();
    quality = new Characteristic::AirQuality(0);
    pm25 = new Characteristic::PM25Density(0);
    pm10 = new Characteristic::PM10Density(0);
    pmFault = new Characteristic::StatusFault(1);
    // Append services to preserve every existing accessory/characteristic IID.
    new Service::AirQualitySensor();
    new Characteristic::Name("VOC Air Quality");
    new Characteristic::ConfiguredName("VOC Air Quality");
    vocQuality = new Characteristic::AirQuality(0);
    vocFault = new Characteristic::StatusFault(1);
    vocIndex = (new Characteristic::VocIndex())->setDescription("VOC Index (unitless)");
    new Service::AirQualitySensor();
    new Characteristic::Name("NOx Air Quality");
    new Characteristic::ConfiguredName("NOx Air Quality");
    noxQuality = new Characteristic::AirQuality(0);
    noxFault = new Characteristic::StatusFault(1);
    noxIndex = (new Characteristic::NoxIndex())->setDescription("NOx Index (unitless)");
  }

  void loop() override {
    if (uint32_t(millis() - lastUpdate) < 5000) return;
    lastUpdate = millis();
    Reading r;
    portENTER_CRITICAL(&readingMux);
    r = latest;
    portEXIT_CRITICAL(&readingMux);
    const bool isFresh = fresh(r.available, millis(), r.at);
    const bool t = isFresh && valid(r.temperature, -40, 125);
    const bool h = isFresh && valid(r.humidity, 0, 100);
    const bool c = isFresh && valid(r.co2, 0, 10000);
    const bool p25 = isFresh && valid(r.pm25, 0, 1000);
    const bool p10 = isFresh && valid(r.pm10, 0, 1000);
    temperatureFault->setVal(!t);
    humidityFault->setVal(!h);
    co2Fault->setVal(!c);
    pmFault->setVal(!(p25 && p10));
    if (t) temperature->setVal(r.temperature); // Always Celsius, regardless of display units.
    if (h) humidity->setVal(r.humidity);
    if (c) {
      co2->setVal(r.co2);
      co2Detected->setVal(r.co2 >= 1000);
    }
    if (p25) pm25->setVal(r.pm25);
    if (p10) pm10->setVal(r.pm10);
    quality->setVal(p25 ? airQuality(r.pm25) : 0);
    const bool v = isFresh && validGasIndex(r.voc);
    const bool n = isFresh && validGasIndex(r.nox);
    vocFault->setVal(!v);
    noxFault->setVal(!n);
    vocQuality->setVal(v ? vocAirQuality(r.voc) : 0);
    noxQuality->setVal(n ? noxAirQuality(r.nox) : 0);
    if (v) vocIndex->setVal(r.voc);
    if (n) noxIndex->setVal(r.nox);
    if (v && n && !gasReadingsLogged) {
      Serial.printf("HomeKit gas indices active: VOC=%.2f (rating %d), NOx=%.2f (rating %d)\n",
                    r.voc, vocAirQuality(r.voc), r.nox, noxAirQuality(r.nox));
      gasReadingsLogged = true;
    }
    // Invalid measurements retain their last value with StatusFault set.
  }
};
} // namespace

void begin(const String &serial, const char *version, void (*onConnected)(int)) {
  if (started) return;
  // AirGradient provisions Wi-Fi. Reuse its credentials; no additional onboarding UI.
  String ssid = WiFi.SSID(), password = WiFi.psk();
  homeSpan.setWifiCredentials(ssid.c_str(), password.c_str());
  homeSpan.setPortNum(1201); // AirGradient's local HTTP API already owns port 80.
  homeSpan.setSerialInputDisable(true);
  homeSpan.setSketchVersion(version);
  homeSpan.setHostNameSuffix(serial.c_str());
  homeSpan.setWifiCallbackAll(onConnected);
  // Deliberately leave HomeSpan's status LED and control button unassigned.
  Preferences prefs;
  prefs.begin("AG_HOMEKIT", false);
  String code = prefs.getString("code", "");
  if (code.length() != 8 || code == "12345678" || code == "87654321") {
    // Distinct digits avoid HomeKit's forbidden repeated/sequential setup codes.
    char digits[] = "0123456789";
    for (int i = 9; i > 0; --i) {
      int j = esp_random() % (i + 1);
      char tmp = digits[i]; digits[i] = digits[j]; digits[j] = tmp;
    }
    code = String(digits).substring(0, 8);
    // These two permutations are explicitly forbidden by HAP.
    if (code == "12345678" || code == "87654321") {
      char tmp = digits[0]; digits[0] = digits[1]; digits[1] = tmp;
      code = String(digits).substring(0, 8);
    }
    prefs.putString("code", code);
  }
  prefs.end();
  homeSpan.setPairingCode(code.c_str());
  Serial.printf("Apple Home setup code: %s-%s-%s\n", code.substring(0,3).c_str(),
                code.substring(3,5).c_str(), code.substring(5).c_str());
  String name = "AirGradient ONE " + serial.substring(serial.length() - 6);
  homeSpan.begin(Category::Sensors, name.c_str(), "airgradient-", "AirGradient ONE");
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name(name.c_str());
  new Characteristic::Manufacturer("AirGradient");
  new Characteristic::Model("ONE I-9PSL");
  new Characteristic::SerialNumber(serial.c_str());
  new Characteristic::FirmwareRevision(version);
  new Sensors();
  started = true;
  // HomeKit continues serving during upstream display animations and calibration waits.
  // Only this task touches characteristics; the sensor loop publishes a locked snapshot.
  homeSpan.autoPoll();
}

void publish(Measurements &m) {
  if (!started) return;
  Reading r;
  const auto raw = m.getMeasures();
  // Gate correction on raw validity: an offset must never turn a sentinel into a reading.
  if (valid(raw.temperature[0], -40, 125))
    r.temperature = m.getCorrectedTempHum(Measurements::Temperature);
  if (valid(raw.humidity[0], 0, 100))
    r.humidity = m.getCorrectedTempHum(Measurements::Humidity);
  r.co2 = raw.co2;
  if (valid(raw.pm_25[0], 0, 1000))
    r.pm25 = m.getCorrectedPM25(true);
  // PM10 is available in getMeasures(), but not in getAverage().
  r.pm10 = raw.pm_10[0];
  r.voc = raw.tvoc;
  r.nox = raw.nox;
  r.at = millis();
  r.available = true;
  portENTER_CRITICAL(&readingMux);
  latest = r;
  portEXIT_CRITICAL(&readingMux);
}

void reset() {
  if (started) vTaskSuspend(homeSpan.getAutoPollTask());
  // The caller immediately restarts after resetting AirGradient configuration.
  const char *names[] = {"HAP", "SRP", "WIFI", "CHAR", "OTA", "AG_HOMEKIT"};
  for (const char *name : names) {
    Preferences prefs;
    if (prefs.begin(name, false)) { prefs.clear(); prefs.end(); }
  }
}
} // namespace homekit
#endif
