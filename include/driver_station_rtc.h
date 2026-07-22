#ifndef DRIVER_STATION_RTC_H
#define DRIVER_STATION_RTC_H

#if defined(_WIN32)
#  if defined(DRIVER_STATION_RTC_BUILDING_LIBRARY)
#    define DRIVER_STATION_RTC_API __declspec(dllexport)
#  else
#    define DRIVER_STATION_RTC_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define DRIVER_STATION_RTC_API __attribute__((visibility("default")))
#else
#  define DRIVER_STATION_RTC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Returns a process-lifetime string describing the linked dependency versions. */
DRIVER_STATION_RTC_API const char* DriverStationRtc_GetDependencyVersions(void);

/** Exercises one symbol from each dependency. Returns zero on success. */
DRIVER_STATION_RTC_API int DriverStationRtc_RunDependencySmokeTest(void);

#ifdef __cplusplus
}
#endif

#endif
