#include "global.h"
#include "constants/weather.h"
#include "coord_event_weather.h"
#include "field_weather.h"

struct CoordEventWeather
{
    u8 param;  // Weather or intensity
    void (*func)(void);
};

static void CoordEventWeather_Clouds(void);
static void CoordEventWeather_Sunny(void);
static void CoordEventWeather_Rain(void);
static void CoordEventWeather_Snow(void);
static void CoordEventWeather_HorizontalFog(void);
static void CoordEventWeather_DiagonalFog(void);
static void CoordEventWeather_Ash(void);
static void CoordEventWeather_Sandstorm(void);
static void CoordEventWeather_Drought(void);
static void CoordEventWeather_Abnormal(void);
static void CoordEventWeather_Route119Cycle(void);
static void CoordEventWeather_Route123Cycle(void);

static void CoordEventWeatherIntensity_Low(void);
static void CoordEventWeatherIntensity_Mild(void);
static void CoordEventWeatherIntensity_Strong(void);
static void CoordEventWeatherIntensity_Extreme(void);

static const struct CoordEventWeather sCoordEventWeatherFuncs[] =
{
    { WEATHER_SUNNY_CLOUDS,      CoordEventWeather_Clouds },
    { WEATHER_SUNNY,             CoordEventWeather_Sunny },
    { WEATHER_RAIN,              CoordEventWeather_Rain },
    { WEATHER_SNOW,              CoordEventWeather_Snow },
    { WEATHER_FOG_HORIZONTAL,    CoordEventWeather_HorizontalFog },
    { WEATHER_VOLCANIC_ASH,      CoordEventWeather_Ash },
    { WEATHER_SANDSTORM,         CoordEventWeather_Sandstorm },
    { WEATHER_FOG_DIAGONAL,      CoordEventWeather_DiagonalFog },
    { WEATHER_DROUGHT,           CoordEventWeather_Drought },
    { WEATHER_ABNORMAL,          CoordEventWeather_Abnormal },
    { WEATHER_ROUTE119_CYCLE,    CoordEventWeather_Route119Cycle },
    { WEATHER_ROUTE123_CYCLE,    CoordEventWeather_Route123Cycle },
};

static const struct CoordEventWeather sCoordEventWeatherIntensityFuncs[] =
{
    { WTHR_INTENSITY_LOW,        CoordEventWeatherIntensity_Low },
    { WTHR_INTENSITY_MILD,       CoordEventWeatherIntensity_Mild },
    { WTHR_INTENSITY_STRONG,     CoordEventWeatherIntensity_Strong },
    { WTHR_INTENSITY_EXTREME,    CoordEventWeatherIntensity_Extreme },
};

static void CoordEventWeather_Clouds(void)
{
    SetWeather(WEATHER_SUNNY_CLOUDS);
}

static void CoordEventWeather_Sunny(void)
{
    SetWeather(WEATHER_SUNNY);
}

static void CoordEventWeather_Rain(void)
{
    SetWeather(WEATHER_RAIN);
}

static void CoordEventWeather_Snow(void)
{
    SetWeather(WEATHER_SNOW);
}

static void CoordEventWeather_HorizontalFog(void)
{
    SetWeather(WEATHER_FOG_HORIZONTAL);
}

static void CoordEventWeather_DiagonalFog(void)
{
    SetWeather(WEATHER_FOG_DIAGONAL);
}

static void CoordEventWeather_Ash(void)
{
    SetWeather(WEATHER_VOLCANIC_ASH);
}

static void CoordEventWeather_Sandstorm(void)
{
    SetWeather(WEATHER_SANDSTORM);
}

static void CoordEventWeather_Drought(void)
{
    SetWeather(WEATHER_DROUGHT);
}

static void CoordEventWeather_Abnormal(void)
{
    SetWeather(WEATHER_ABNORMAL);
}

static void CoordEventWeather_Route119Cycle(void)
{
    SetWeather(WEATHER_ROUTE119_CYCLE);
}

static void CoordEventWeather_Route123Cycle(void)
{
    SetWeather(WEATHER_ROUTE123_CYCLE);
}

static void CoordEventWeatherIntensity_Low(void)
{
    SetWeatherIntensity(WTHR_INTENSITY_LOW);
}

static void CoordEventWeatherIntensity_Mild(void)
{
    SetWeatherIntensity(WTHR_INTENSITY_MILD);
}

static void CoordEventWeatherIntensity_Strong(void)
{
    SetWeatherIntensity(WTHR_INTENSITY_STRONG);
}

static void CoordEventWeatherIntensity_Extreme(void)
{
    SetWeatherIntensity(WTHR_INTENSITY_EXTREME);
}


void DoCoordEventWeather(u8 weather)
{
    u8 i;

    if (weather == 0xFF)
        return;

    for (i = 0; i < ARRAY_COUNT(sCoordEventWeatherFuncs); i++)
    {
        if (sCoordEventWeatherFuncs[i].param == weather)
        {
            sCoordEventWeatherFuncs[i].func();
            return;
        }
    }
}

void DoCoordEventWeatherIntensity(u8 intensity)
{
    u8 i;

    if (intensity == 0xFF)
        return;

    for (i = 0; i < ARRAY_COUNT(sCoordEventWeatherIntensityFuncs); i++)
    {
        if (sCoordEventWeatherIntensityFuncs[i].param == intensity)
        {
            sCoordEventWeatherIntensityFuncs[i].func();
            return;
        }
    }
}
