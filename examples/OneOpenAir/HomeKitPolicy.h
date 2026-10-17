#pragma once
#include <cmath>
#include <cstdint>

namespace homekit {
inline bool valid(float v, float low, float high) {
  return std::isfinite(v) && v >= low && v <= high;
}
inline bool fresh(bool available, uint32_t now, uint32_t at) {
  return available && uint32_t(now - at) < 30000;
}
// PM2.5 concentration bands in ug/m3; HomeKit's five qualitative categories.
inline int airQuality(float pm25) {
  if (!valid(pm25, 0, 1000)) return 0;
  if (pm25 <= 9.0f) return 1;
  if (pm25 <= 35.4f) return 2;
  if (pm25 <= 55.4f) return 3;
  if (pm25 <= 125.4f) return 4;
  return 5;
}
// SGP41 reports 1..500; zero is the algorithm's warm-up/unavailable output.
inline bool validGasIndex(float index) { return valid(index, 1, 500); }

// Relative activity categories, NOT health limits or concentration conversions.
// AirGradient dashboard bands, with green split at Sensirion's baseline.
inline int vocAirQuality(float index) {
  if (!validGasIndex(index)) return 0;
  if (index <= 100) return 1;
  if (index <= 150) return 2;
  if (index <= 250) return 3;
  if (index <= 400) return 4;
  return 5;
}
inline int noxAirQuality(float index) {
  if (!validGasIndex(index)) return 0;
  if (index <= 1) return 1;
  if (index <= 20) return 2;
  if (index <= 150) return 3;
  if (index <= 300) return 4;
  return 5;
}
} // namespace homekit
