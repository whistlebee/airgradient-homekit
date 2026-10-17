#include "../examples/OneOpenAir/HomeKitPolicy.h"
#include <cassert>
#include <limits>
#include <initializer_list>
int main() {
  using namespace homekit;
  // Missing sensors must never turn into valid zeroes or excellent air quality.
  assert(!valid(-1000, -40, 125));
  assert(!valid(-1, 0, 100));
  assert(!valid(std::numeric_limits<float>::quiet_NaN(), 0, 1000));
  assert(!valid(std::numeric_limits<float>::infinity(), 0, 1000));
  assert(airQuality(-1) == 0);
  assert(airQuality(NAN) == 0);
  assert(airQuality(0) == 1 && airQuality(9) == 1);
  assert(airQuality(9.1f) == 2 && airQuality(35.4f) == 2);
  assert(airQuality(35.5f) == 3 && airQuality(55.4f) == 3);
  assert(airQuality(55.5f) == 4 && airQuality(125.4f) == 4);
  assert(airQuality(125.5f) == 5 && airQuality(1000) == 5);
  // Warm-up, disconnection, and non-finite values must not report clean air.
  for (float index : {-1.0f, 0.0f, 0.5f, 500.1f, NAN, INFINITY}) {
    assert(!validGasIndex(index));
    assert(vocAirQuality(index) == 0);
    assert(noxAirQuality(index) == 0);
  }
  assert(vocAirQuality(1) == 1 && vocAirQuality(100) == 1);
  assert(vocAirQuality(100.1f) == 2 && vocAirQuality(150) == 2);
  assert(vocAirQuality(150.1f) == 3 && vocAirQuality(250) == 3);
  assert(vocAirQuality(250.1f) == 4 && vocAirQuality(400) == 4);
  assert(vocAirQuality(400.1f) == 5 && vocAirQuality(500) == 5);
  assert(noxAirQuality(1) == 1 && noxAirQuality(1.1f) == 2);
  assert(noxAirQuality(20) == 2 && noxAirQuality(20.1f) == 3);
  assert(noxAirQuality(150) == 3 && noxAirQuality(150.1f) == 4);
  assert(noxAirQuality(300) == 4 && noxAirQuality(300.1f) == 5);
  assert(noxAirQuality(500) == 5);
  assert(!fresh(false, 0, 0));
  assert(fresh(true, 29999, 0));
  assert(!fresh(true, 30000, 0));
  // millis wraps after ~49 days; freshness must still work across rollover.
  assert(fresh(true, 499, UINT32_MAX - 500));
  assert(!fresh(true, 29999, UINT32_MAX));
}
