#include "driver_station_rtc.h"

#include <stdio.h>

int main(void) {
    puts(DriverStationRtc_GetDependencyVersions());
    return DriverStationRtc_RunDependencySmokeTest();
}
