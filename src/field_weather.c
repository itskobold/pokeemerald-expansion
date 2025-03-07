#include "global.h"
#include "constants/songs.h"
#include "constants/weather.h"
#include "constants/rgb.h"
#include "util.h"
#include "event_object_movement.h"
#include "field_weather.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "random.h"
#include "script.h"
#include "start_menu.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "trig.h"
#include "gpu_regs.h"

#define SUNNY_COLOR_INDEX(color) ((((color) >> 1) & 0xF) | (((color) >> 2) & 0xF0) | (((color) >> 3) & 0xF00))

enum
{
    COLOR_MAP_NONE,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_CONTRAST,
};

struct RGBColor
{
    u16 r:5;
    u16 g:5;
    u16 b:5;
};

struct WeatherCallbacks
{
    void (*initVars)(void);
    void (*intensity)(void);
    void (*main)(void);
    void (*initAll)(void);
    bool8 (*finish)(void);
};

// This file's functions.
static bool8 LightenSpritePaletteInFog(u8);
static void BuildColorMaps(void);
static void UpdateWeatherColorMap(void);
static void ApplyColorMap(u8 startPalIndex, u8 numPalettes, s8 colorMapIndex);
static void ApplyColorMapWithBlend(u8 startPalIndex, u8 numPalettes, s8 colorMapIndex, u8 blendCoeff, u16 blendColor);
static void ApplySunnyColorMapWithBlend(s8 colorMapIndex, u8 blendCoeff, u16 blendColor);
static void ApplyFogBlend(u8 blendCoeff, u16 blendColor);
static void DoFadeInScreen_NoEffect(void);
static bool8 FadeInScreen_Shade(void);
static void DoFadeInScreen_Shade(void);
static bool8 FadeInScreen_Sunny(void);
static bool8 FadeInScreen_FogHorizontal(void);
static void FadeInScreenWithWeather(void);
static void DoNothing(void);
static void Task_WeatherInit(u8 taskId);
static void Task_WeatherMain(u8 taskId);
static void None_Init(void);
static void None_Main(void);
static u8 None_Finish(void);

EWRAM_DATA struct Weather gWeather = {0};
EWRAM_DATA static u8 sFieldEffectPaletteColorMapTypes[32] = {0};

static const u8 *sPaletteColorMapTypes;

// The sunny weather effect uses a precalculated color lookup table. Presumably this
// is because the underlying color shift calculation is slow.
static const u16 sSunnyWeatherColors[][0x1000] = {
    INCBIN_U16("graphics/weather/sunny/colors_0.bin"),
    INCBIN_U16("graphics/weather/sunny/colors_1.bin"),
    INCBIN_U16("graphics/weather/sunny/colors_2.bin"),
    INCBIN_U16("graphics/weather/sunny/colors_3.bin"),
    INCBIN_U16("graphics/weather/sunny/colors_4.bin"),
    INCBIN_U16("graphics/weather/sunny/colors_5.bin"),
};

// This is a pointer to gWeather. All code in this file accesses gWeather directly,
// while code in other field weather files accesses gWeather through this pointer.
// This is likely the result of compiler optimization, since using the pointer in
// this file produces the same result as accessing gWeather directly.
struct Weather *const gWeatherPtr = &gWeather;

static const struct WeatherCallbacks sWeatherFuncs[] =
{
    [WEATHER_NONE]               = {None_Init,              None_Main,        None_Main,          None_Init,             None_Finish},
    [WEATHER_SUNNY_CLOUDS]       = {Clouds_InitVars,        None_Main,        Clouds_Main,        Clouds_InitAll,        Clouds_Finish},
    [WEATHER_SUNNY]              = {Sunny_InitVars,         Sunny_Intensity,  Sunny_Main,         Sunny_InitAll,         Sunny_Finish},
    [WEATHER_NORMAL]             = {Normal_InitVars,        Normal_Intensity, Normal_Main,        Normal_InitAll,        Normal_Finish},
    [WEATHER_RAIN]               = {Rain_InitVars,          Rain_Intensity,   Rain_Main,          Rain_InitAll,          Rain_Finish},
    [WEATHER_SNOW]               = {Snow_InitVars,          None_Main,        Snow_Main,          Snow_InitAll,          Snow_Finish},
    [WEATHER_FOG_HORIZONTAL]     = {FogHorizontal_InitVars, None_Main,        FogHorizontal_Main, FogHorizontal_InitAll, FogHorizontal_Finish},
    [WEATHER_VOLCANIC_ASH]       = {Ash_InitVars,           None_Main,        Ash_Main,           Ash_InitAll,           Ash_Finish},
    [WEATHER_SANDSTORM]          = {Sandstorm_InitVars,     None_Main,        Sandstorm_Main,     Sandstorm_InitAll,     Sandstorm_Finish},
    [WEATHER_FOG_DIAGONAL]       = {FogDiagonal_InitVars,   None_Main,        FogDiagonal_Main,   FogDiagonal_InitAll,   FogDiagonal_Finish},
    [WEATHER_UNDERWATER_BUBBLES] = {Bubbles_InitVars,       None_Main,        Bubbles_Main,       Bubbles_InitAll,       Bubbles_Finish},
};

void (*const gWeatherPalStateFuncs[])(void) =
{
    [WEATHER_PAL_STATE_CHANGING_WEATHER]  = UpdateWeatherColorMap,
    [WEATHER_PAL_STATE_SCREEN_FADING_IN]  = FadeInScreenWithWeather,
    [WEATHER_PAL_STATE_SCREEN_FADING_OUT] = DoNothing,
    [WEATHER_PAL_STATE_IDLE]              = DoNothing,
};

// This table specifies which of the color maps should be
// applied to each of the background and sprite palettes.
static const u8 sBasePaletteColorMapTypes[32] =
{
    // background palettes
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_NONE,
    COLOR_MAP_NONE,
    // sprite palettes
    COLOR_MAP_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_CONTRAST,
    COLOR_MAP_CONTRAST,
    COLOR_MAP_CONTRAST,
    COLOR_MAP_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
    COLOR_MAP_DARK_CONTRAST,
};

const u16 gFogPalette[] = INCBIN_U16("graphics/weather/fog.gbapal");

static u8 GetRandomAbnormalWeather(void);
void StartWeather(void)
{
    if (!FuncIsActiveTask(Task_WeatherMain))
    {
        u8 index = AllocSpritePalette(PALTAG_WEATHER);
        CpuCopy32(gFogPalette, &gPlttBufferUnfaded[0x100 + index * 16], 32);
        BuildColorMaps();
        gWeatherPtr->contrastColorMapSpritePalIndex = index;
        gWeatherPtr->weatherPicSpritePalIndex = AllocSpritePalette(PALTAG_WEATHER_2);
        gWeatherPtr->rainSpriteCount = 0;
        gWeatherPtr->rainSpriteVisibleCounter = 0;
        gWeatherPtr->updatingRainSprites = FALSE;
        gWeatherPtr->rainSEPlaying = 0;
        gWeatherPtr->curRainSpriteIndex = 0;
        gWeatherPtr->cloudSpritesCreated = 0;
        gWeatherPtr->snowflakeSpriteCount = 0;
        gWeatherPtr->ashSpritesCreated = 0;
        gWeatherPtr->fogHSpritesCreated = 0;
        gWeatherPtr->fogDSpritesCreated = 0;
        gWeatherPtr->sandstormSpritesCreated = 0;
        gWeatherPtr->sandstormSwirlSpritesCreated = 0;
        gWeatherPtr->bubblesSpritesCreated = 0;
        gWeatherPtr->lightenedFogSpritePalsCount = 0;
        Weather_SetBlendCoeffs(16, 0);
        gWeatherPtr->currWeather = WEATHER_NONE;
        gWeatherPtr->currIntensity = WTHR_INTENSITY_LOW;
        gWeatherPtr->nextAbnormalWeather = GetRandomAbnormalWeather();  // Init with random abnormal weather
        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
        gWeatherPtr->readyForInit = FALSE;
        gWeatherPtr->weatherChangeComplete = TRUE;
        gWeatherPtr->taskId = CreateTask(Task_WeatherInit, 80);
    }
}

void SetNextWeather(u8 weather)
{
    if (gWeatherPtr->nextWeather != weather && gWeatherPtr->currWeather == weather)
        sWeatherFuncs[weather].initVars();

    gWeatherPtr->nextWeather = weather;
    gWeatherPtr->finishStep = 0;
}

void SetCurrentAndNextWeather(u8 weather)
{
    gWeatherPtr->currWeather = weather;
    gWeatherPtr->nextWeather = weather;
}

void SetCurrentAndNextWeatherIntensity(u8 intensity)
{
    gWeatherPtr->currIntensity = intensity;
    gWeatherPtr->nextIntensity = intensity;
}

void SetCurrentAndNextWeatherNoDelay(u8 weather)
{
    gWeatherPtr->currWeather = weather;
    gWeatherPtr->nextWeather = weather;
    // Overrides the normal delay during screen fading.
    gWeatherPtr->readyForInit = TRUE;
}

void SetNextWeatherIntensity(u8 intensity)
{
    gWeatherPtr->nextIntensity = intensity;
}

void DoCoordEventWeather(u8 weather)
{
    if (weather < WEATHER_NO_CHANGE)
        SetWeather(weather);
}

void DoCoordEventWeatherIntensity(u8 intensity)
{
    if (intensity < WTHR_INTENSITY_NO_CHANGE)
        SetWeatherIntensity(intensity);
}

// 50/50 chance of being strong rain or extreme sun
static u8 GetRandomAbnormalWeather(void)
{
    if (Random() % 2)
        return WEATHER_RAIN;
    else
        return WEATHER_SUNNY;
}

static void Task_WeatherInit(u8 taskId)
{
    // Waits until it's ok to initialize weather.
    // When the screen fades in, this is set to TRUE.
    if (gWeatherPtr->readyForInit)
    {
        sWeatherFuncs[gWeatherPtr->currWeather].initAll();
        gTasks[taskId].func = Task_WeatherMain;
    }
}

static void Task_WeatherMain(u8 taskId)
{
    // Changing weather
    if (gWeatherPtr->currWeather != gWeatherPtr->nextWeather)
    {
        gWeatherPtr->weatherChangeComplete = FALSE;
        if (!sWeatherFuncs[gWeatherPtr->currWeather].finish()
            && gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_SCREEN_FADING_OUT)
        {
            // Finished cleaning up previous weather. Now transition to next weather.
            sWeatherFuncs[gWeatherPtr->nextWeather].initVars();
            gWeatherPtr->colorMapStepCounter = 0;
            gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_CHANGING_WEATHER;
            gWeatherPtr->currWeather = gWeatherPtr->nextWeather;
            gWeatherPtr->weatherChangeComplete = TRUE;
        }
    }
    else
        sWeatherFuncs[gWeatherPtr->currWeather].main();

    // Changing weather intensity and weather isn't changing
    if (gWeatherPtr->currIntensity != gWeatherPtr->nextIntensity && gWeatherPtr->weatherChangeComplete && gWeatherPtr->weatherGfxLoaded)
    {
        sWeatherFuncs[gWeatherPtr->currWeather].intensity();
        gWeatherPtr->currIntensity = gWeatherPtr->nextIntensity;
    }

    // Update palette processing state
    gWeatherPalStateFuncs[gWeatherPtr->palProcessingState]();
}

static void None_Init(void)
{
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 0;
}

static void None_Main(void)
{
}

static u8 None_Finish(void)
{
    return 0;
}

// Builds two tables that contain color maps, used for directly transforming
// palette colors in weather effects. The colors maps are a spectrum of
// brightness + contrast mappings. By transitioning between the maps, weather
// effects like lightning are created.
// It's unclear why the two tables aren't declared as const arrays, since
// this function always builds the same two tables.
static void BuildColorMaps(void)
{
    u16 i;
    u8 (*colorMaps)[32];
    u16 colorVal;
    u16 curBrightness;
    u16 brightnessDelta;
    u16 colorMapIndex;
    u16 baseBrightness;
    u32 remainingBrightness;
    s16 diff;

    sPaletteColorMapTypes = sBasePaletteColorMapTypes;
    for (i = 0; i < 2; i++)
    {
        if (i == 0)
            colorMaps = gWeatherPtr->darkenedContrastColorMaps;
        else
            colorMaps = gWeatherPtr->contrastColorMaps;

        for (colorVal = 0; colorVal < 32; colorVal++)
        {
            curBrightness = colorVal << 8;
            if (i == 0)
                brightnessDelta = (colorVal << 8) / 16;
            else
                brightnessDelta = 0;

            // First three color mappings are simple brightness modifiers which are
            // progressively darker, according to brightnessDelta.
            for (colorMapIndex = 0; colorMapIndex < 3; colorMapIndex++)
            {
                curBrightness -= brightnessDelta;
                colorMaps[colorMapIndex][colorVal] = curBrightness >> 8;
            }

            baseBrightness = curBrightness;
            remainingBrightness = 0x1f00 - curBrightness;
            if ((0x1f00 - curBrightness) < 0)
                remainingBrightness += 0xf;

            brightnessDelta = remainingBrightness / (NUM_WEATHER_COLOR_MAPS - 3);
            if (colorVal < 12)
            {
                // For shadows (color values < 12), the remaining color mappings are
                // brightness modifiers, which are increased at a significantly lower rate
                // than the midtones and highlights (color values >= 12). This creates a
                // high contrast effect, used in the thunderstorm weather.
                for (; colorMapIndex < NUM_WEATHER_COLOR_MAPS; colorMapIndex++)
                {
                    curBrightness += brightnessDelta;
                    diff = curBrightness - baseBrightness;
                    if (diff > 0)
                        curBrightness -= diff / 2;
                    colorMaps[colorMapIndex][colorVal] = curBrightness >> 8;
                    if (colorMaps[colorMapIndex][colorVal] > 31)
                        colorMaps[colorMapIndex][colorVal] = 31;
                }
            }
            else
            {
                // For midtones and highlights (color values >= 12), the remaining
                // color mappings are simple brightness modifiers which are
                // progressively brighter, hitting exactly 31 at the last mapping.
                for (; colorMapIndex < NUM_WEATHER_COLOR_MAPS; colorMapIndex++)
                {
                    curBrightness += brightnessDelta;
                    colorMaps[colorMapIndex][colorVal] = curBrightness >> 8;
                    if (colorMaps[colorMapIndex][colorVal] > 31)
                        colorMaps[colorMapIndex][colorVal] = 31;
                }
            }
        }
    }
}

// When the weather is changing, it gradually updates the palettes
// towards the desired color map.
static void UpdateWeatherColorMap(void)
{
    if (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_SCREEN_FADING_OUT)
    {
        if (gWeatherPtr->colorMapIndex == gWeatherPtr->targetColorMapIndex)
        {
            gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
        }
        else
        {
            if (++gWeatherPtr->colorMapStepCounter >= gWeatherPtr->colorMapStepDelay)
            {
                gWeatherPtr->colorMapStepCounter = 0;
                if (gWeatherPtr->colorMapIndex < gWeatherPtr->targetColorMapIndex)
                    gWeatherPtr->colorMapIndex++;
                else
                    gWeatherPtr->colorMapIndex--;

                ApplyColorMap(0, 32, gWeatherPtr->colorMapIndex);
            }
        }
    }
}

static void FadeInScreenWithWeather(void)
{
    if (++gWeatherPtr->fadeInTimer > 1)
        gWeatherPtr->fadeInFirstFrame = FALSE;

    switch (gWeatherPtr->currWeather)
    {
    case WEATHER_NORMAL:
        if (gWeatherPtr->currIntensity == WTHR_INTENSITY_EXTREME)
            DoFadeInScreen_Shade();
        else
            DoFadeInScreen_NoEffect();
        break;
    case WEATHER_RAIN:
    case WEATHER_SNOW:
        DoFadeInScreen_Shade();
        break;
    case WEATHER_SUNNY:
        if (FadeInScreen_Sunny() == FALSE)
        {
            gWeatherPtr->colorMapIndex = -6;
            gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
        }
        break;
    case WEATHER_FOG_HORIZONTAL:
        if (FadeInScreen_FogHorizontal() == FALSE)
        {
            gWeatherPtr->colorMapIndex = 0;
            gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
        }
        break;
    case WEATHER_VOLCANIC_ASH:
    case WEATHER_SANDSTORM:
    case WEATHER_FOG_DIAGONAL:
    default:
        DoFadeInScreen_NoEffect();
        break;
    }
}

static void DoFadeInScreen_NoEffect(void)
{
    if (!gPaletteFade.active)
    {
        gWeatherPtr->colorMapIndex = 0; // gWeatherPtr->targetColorMapIndex;
        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
    }
}

static bool8 FadeInScreen_Shade(void)
{
    if (gWeatherPtr->fadeScreenCounter == 16)
        return FALSE;

    if (++gWeatherPtr->fadeScreenCounter >= 16)
    {
        ApplyColorMap(0, 32, 3);
        gWeatherPtr->fadeScreenCounter = 16;
        return FALSE;
    }

    ApplyColorMapWithBlend(0, 32, 3, 16 - gWeatherPtr->fadeScreenCounter, gWeatherPtr->fadeDestColor);
    return TRUE;
}

static void DoFadeInScreen_Shade(void)
{
    if (FadeInScreen_Shade() == FALSE)
    {
        gWeatherPtr->colorMapIndex = 3;
        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
    }
}

static bool8 FadeInScreen_Sunny(void)
{
    if (gWeatherPtr->fadeScreenCounter == 16)
        return FALSE;

    if (++gWeatherPtr->fadeScreenCounter >= 16)
    {
        ApplyColorMap(0, 32, -6);
        gWeatherPtr->fadeScreenCounter = 16;
        return FALSE;
    }

    ApplySunnyColorMapWithBlend(-6, 16 - gWeatherPtr->fadeScreenCounter, gWeatherPtr->fadeDestColor);
    return TRUE;
}

static bool8 FadeInScreen_FogHorizontal(void)
{
    if (gWeatherPtr->fadeScreenCounter == 16)
        return FALSE;

    gWeatherPtr->fadeScreenCounter++;
    ApplyFogBlend(16 - gWeatherPtr->fadeScreenCounter, gWeatherPtr->fadeDestColor);
    return TRUE;
}

static void DoNothing(void)
{ }

static void ApplyColorMap(u8 startPalIndex, u8 numPalettes, s8 colorMapIndex)
{
    u16 curPalIndex;
    u16 palOffset;
    u8 *colorMap;
    u16 i;

    if (colorMapIndex > 0)
    {
        colorMapIndex--;
        palOffset = startPalIndex * 16;
        numPalettes += startPalIndex;
        curPalIndex = startPalIndex;

        // Loop through the specified palette range and apply necessary color maps.
        while (curPalIndex < numPalettes)
        {
            if (sPaletteColorMapTypes[curPalIndex] == COLOR_MAP_NONE)
            {
                // No palette change.
                CpuFastCopy(gPlttBufferUnfaded + palOffset, gPlttBufferFaded + palOffset, 16 * sizeof(u16));
                palOffset += 16;
            }
            else
            {
                u8 r, g, b;

                if (sPaletteColorMapTypes[curPalIndex] == COLOR_MAP_CONTRAST || curPalIndex - 16 == gWeatherPtr->contrastColorMapSpritePalIndex)
                    colorMap = gWeatherPtr->contrastColorMaps[colorMapIndex];
                else
                    colorMap = gWeatherPtr->darkenedContrastColorMaps[colorMapIndex];

                for (i = 0; i < 16; i++)
                {
                    // Apply color map to the original color.
                    struct RGBColor baseColor = *(struct RGBColor *)&gPlttBufferUnfaded[palOffset];
                    r = colorMap[baseColor.r];
                    g = colorMap[baseColor.g];
                    b = colorMap[baseColor.b];
                    gPlttBufferFaded[palOffset++] = RGB2(r, g, b);
                }
            }

            curPalIndex++;
        }
    }
    else if (colorMapIndex < 0)
    {
        // A negative gammIndex value means that the blending will come from the special sunny weather's palette tables.
        colorMapIndex = -colorMapIndex - 1;
        palOffset = startPalIndex * 16;
        numPalettes += startPalIndex;
        curPalIndex = startPalIndex;

        while (curPalIndex < numPalettes)
        {
            if (sPaletteColorMapTypes[curPalIndex] == COLOR_MAP_NONE)
            {
                // No palette change.
                CpuFastCopy(gPlttBufferUnfaded + palOffset, gPlttBufferFaded + palOffset, 16 * sizeof(u16));
                palOffset += 16;
            }
            else
            {
                for (i = 0; i < 16; i++)
                {
                    gPlttBufferFaded[palOffset] = sSunnyWeatherColors[colorMapIndex][SUNNY_COLOR_INDEX(gPlttBufferUnfaded[palOffset])];
                    palOffset++;
                }
            }

            curPalIndex++;
        }
    }
    else
    {
        // No palette blending.
        CpuFastCopy(gPlttBufferUnfaded + startPalIndex * 16, gPlttBufferFaded + startPalIndex * 16, numPalettes * 16 * sizeof(u16));
    }
}

static void ApplyColorMapWithBlend(u8 startPalIndex, u8 numPalettes, s8 colorMapIndex, u8 blendCoeff, u16 blendColor)
{
    u16 palOffset;
    u16 curPalIndex;
    u16 i;
    struct RGBColor color = *(struct RGBColor *)&blendColor;
    u8 rBlend = color.r;
    u8 gBlend = color.g;
    u8 bBlend = color.b;

    palOffset = BG_PLTT_ID(startPalIndex);
    numPalettes += startPalIndex;
    colorMapIndex--;
    curPalIndex = startPalIndex;

    while (curPalIndex < numPalettes)
    {
        if (sPaletteColorMapTypes[curPalIndex] == COLOR_MAP_NONE)
        {
            // No color map. Simply blend the colors.
            BlendPalette(palOffset, 16, blendCoeff, blendColor);
            palOffset += 16;
        }
        else
        {
            u8 *colorMap;

            if (sPaletteColorMapTypes[curPalIndex] == COLOR_MAP_DARK_CONTRAST)
                colorMap = gWeatherPtr->darkenedContrastColorMaps[colorMapIndex];
            else
                colorMap = gWeatherPtr->contrastColorMaps[colorMapIndex];

            for (i = 0; i < 16; i++)
            {
                struct RGBColor baseColor = *(struct RGBColor *)&gPlttBufferUnfaded[palOffset];
                u8 r = colorMap[baseColor.r];
                u8 g = colorMap[baseColor.g];
                u8 b = colorMap[baseColor.b];

                // Apply color map and target blend color to the original color.
                r += ((rBlend - r) * blendCoeff) >> 4;
                g += ((gBlend - g) * blendCoeff) >> 4;
                b += ((bBlend - b) * blendCoeff) >> 4;
                gPlttBufferFaded[palOffset++] = RGB2(r, g, b);
            }
        }

        curPalIndex++;
    }
}

static void ApplySunnyColorMapWithBlend(s8 colorMapIndex, u8 blendCoeff, u16 blendColor)
{
    struct RGBColor color;
    u8 rBlend;
    u8 gBlend;
    u8 bBlend;
    u16 curPalIndex;
    u16 palOffset;
    u16 i;

    colorMapIndex = -colorMapIndex - 1;
    color = *(struct RGBColor *)&blendColor;
    rBlend = color.r;
    gBlend = color.g;
    bBlend = color.b;
    palOffset = 0;
    for (curPalIndex = 0; curPalIndex < 32; curPalIndex++)
    {
        if (sPaletteColorMapTypes[curPalIndex] == COLOR_MAP_NONE)
        {
            // No color map. Simply blend the colors.
            BlendPalette(palOffset, 16, blendCoeff, blendColor);
            palOffset += 16;
        }
        else
        {
            for (i = 0; i < 16; i++)
            {
                u32 offset;
                struct RGBColor color1;
                struct RGBColor color2;
                u8 r1, g1, b1;
                u8 r2, g2, b2;

                color1 = *(struct RGBColor *)&gPlttBufferUnfaded[palOffset];
                r1 = color1.r;
                g1 = color1.g;
                b1 = color1.b;

                offset = ((b1 & 0x1E) << 7) | ((g1 & 0x1E) << 3) | ((r1 & 0x1E) >> 1);
                color2 = *(struct RGBColor *)&sSunnyWeatherColors[colorMapIndex][offset];
                r2 = color2.r;
                g2 = color2.g;
                b2 = color2.b;

                r2 += ((rBlend - r2) * blendCoeff) >> 4;
                g2 += ((gBlend - g2) * blendCoeff) >> 4;
                b2 += ((bBlend - b2) * blendCoeff) >> 4;

                gPlttBufferFaded[palOffset++] = RGB2(r2, g2, b2);
            }
        }
    }
}

static void ApplyFogBlend(u8 blendCoeff, u16 blendColor)
{
    struct RGBColor color;
    u8 rBlend;
    u8 gBlend;
    u8 bBlend;
    u16 curPalIndex;

    BlendPalette(BG_PLTT_ID(0), 16 * 16, blendCoeff, blendColor);
    color = *(struct RGBColor *)&blendColor;
    rBlend = color.r;
    gBlend = color.g;
    bBlend = color.b;

    for (curPalIndex = 16; curPalIndex < 32; curPalIndex++)
    {
        if (LightenSpritePaletteInFog(curPalIndex))
        {
            u16 palEnd = (curPalIndex + 1) * 16;
            u16 palOffset = curPalIndex * 16;

            while (palOffset < palEnd)
            {
                struct RGBColor color = *(struct RGBColor *)&gPlttBufferUnfaded[palOffset];
                u8 r = color.r;
                u8 g = color.g;
                u8 b = color.b;

                r += ((28 - r) * 3) >> 2;
                g += ((31 - g) * 3) >> 2;
                b += ((28 - b) * 3) >> 2;

                r += ((rBlend - r) * blendCoeff) >> 4;
                g += ((gBlend - g) * blendCoeff) >> 4;
                b += ((bBlend - b) * blendCoeff) >> 4;

                gPlttBufferFaded[palOffset] = RGB2(r, g, b);
                palOffset++;
            }
        }
        else
        {
            BlendPalette(PLTT_ID(curPalIndex), 16, blendCoeff, blendColor);
        }
    }
}

static void MarkFogSpritePalToLighten(u8 paletteIndex)
{
    if (gWeatherPtr->lightenedFogSpritePalsCount < 6)
    {
        gWeatherPtr->lightenedFogSpritePals[gWeatherPtr->lightenedFogSpritePalsCount] = paletteIndex;
        gWeatherPtr->lightenedFogSpritePalsCount++;
    }
}

static bool8 LightenSpritePaletteInFog(u8 paletteIndex)
{
    u16 i;

    for (i = 0; i < gWeatherPtr->lightenedFogSpritePalsCount; i++)
    {
        if (gWeatherPtr->lightenedFogSpritePals[i] == paletteIndex)
            return TRUE;
    }

    return FALSE;
}

void ApplyWeatherColorMapIfIdle(s8 colorMapIndex)
{
    if (gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_IDLE)
    {
        ApplyColorMap(0, 32, colorMapIndex);
        gWeatherPtr->colorMapIndex = colorMapIndex;
    }
}

void ApplyWeatherColorMapIfIdle_Gradual(u8 colorMapIndex, u8 targetColorMapIndex, u8 colorMapStepDelay)
{
    if (gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_IDLE)
    {
        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_CHANGING_WEATHER;
        gWeatherPtr->colorMapIndex = colorMapIndex;
        gWeatherPtr->targetColorMapIndex = targetColorMapIndex;
        gWeatherPtr->colorMapStepCounter = 0;
        gWeatherPtr->colorMapStepDelay = colorMapStepDelay;
        ApplyWeatherColorMapIfIdle(colorMapIndex);
    }
}

void FadeScreen(u8 mode, s8 delay)
{
    u32 fadeColor;
    bool8 fadeOut;
    bool8 useWeatherPal;

    switch (mode)
    {
    case FADE_FROM_BLACK:
        fadeColor = RGB_BLACK;
        fadeOut = FALSE;
        break;
    case FADE_FROM_WHITE:
        fadeColor = RGB_WHITEALPHA;
        fadeOut = FALSE;
        break;
    case FADE_TO_BLACK:
        fadeColor = RGB_BLACK;
        fadeOut = TRUE;
        break;
    case FADE_TO_WHITE:
        fadeColor = RGB_WHITEALPHA;
        fadeOut = TRUE;
        break;
    default:
        return;
    }

    switch (gWeatherPtr->currWeather)
    {
    case WEATHER_NORMAL:
        if (gWeatherPtr->currIntensity == WTHR_INTENSITY_EXTREME)
            useWeatherPal = TRUE;
        else
            useWeatherPal = FALSE;
        break;
    case WEATHER_RAIN:
    case WEATHER_SNOW:
    case WEATHER_FOG_HORIZONTAL:
    case WEATHER_SUNNY:
        useWeatherPal = TRUE;
        break;
    default:
        useWeatherPal = FALSE;
        break;
    }

    if (fadeOut)
    {
        if (useWeatherPal)
            CpuFastCopy(gPlttBufferFaded, gPlttBufferUnfaded, PLTT_BUFFER_SIZE * 2);

        BeginNormalPaletteFade(PALETTES_ALL, delay, 0, 16, fadeColor);
        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_SCREEN_FADING_OUT;
    }
    else
    {
        gWeatherPtr->fadeDestColor = fadeColor;
        if (useWeatherPal)
            gWeatherPtr->fadeScreenCounter = 0;
        else
            BeginNormalPaletteFade(PALETTES_ALL, delay, 16, 0, fadeColor);

        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_SCREEN_FADING_IN;
        gWeatherPtr->fadeInFirstFrame = TRUE;
        gWeatherPtr->fadeInTimer = 0;
        Weather_SetBlendCoeffs(gWeatherPtr->currBlendEVA, gWeatherPtr->currBlendEVB);
        gWeatherPtr->readyForInit = TRUE;
    }
}

bool8 IsWeatherNotFadingIn(void)
{
    return (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_SCREEN_FADING_IN);
}

void UpdateSpritePaletteWithWeather(u8 spritePaletteIndex)
{
    u16 paletteIndex = 16 + spritePaletteIndex;
    u16 i;

    switch (gWeatherPtr->palProcessingState)
    {
    case WEATHER_PAL_STATE_SCREEN_FADING_IN:
        if (gWeatherPtr->fadeInFirstFrame)
        {
            if (gWeatherPtr->currWeather == WEATHER_FOG_HORIZONTAL)
                MarkFogSpritePalToLighten(paletteIndex);
            paletteIndex *= 16;
            for (i = 0; i < 16; i++)
                gPlttBufferFaded[paletteIndex + i] = gWeatherPtr->fadeDestColor;
        }
        break;
    case WEATHER_PAL_STATE_SCREEN_FADING_OUT:
        paletteIndex = PLTT_ID(paletteIndex);
        CpuFastCopy(gPlttBufferFaded + paletteIndex, gPlttBufferUnfaded + paletteIndex, PLTT_SIZE_4BPP);
        BlendPalette(paletteIndex, 16, gPaletteFade.y, gPaletteFade.blendColor);
        break;
    // WEATHER_PAL_STATE_CHANGING_WEATHER
    // WEATHER_PAL_STATE_CHANGING_IDLE
    default:
        if (gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
        {
            ApplyColorMap(paletteIndex, 1, gWeatherPtr->colorMapIndex);
        }
        else
        {
            paletteIndex = PLTT_ID(paletteIndex);
            BlendPalette(paletteIndex, 16, 12, RGB(28, 31, 28));
        }
        break;
    }
}

void ApplyWeatherColorMapToPal(u8 paletteIndex)
{
    ApplyColorMap(paletteIndex, 1, gWeatherPtr->colorMapIndex);
}

void LoadCustomWeatherSpritePalette(const u16 *palette)
{
    LoadPalette(palette, OBJ_PLTT_ID(gWeatherPtr->weatherPicSpritePalIndex), PLTT_SIZE_4BPP);
    UpdateSpritePaletteWithWeather(gWeatherPtr->weatherPicSpritePalIndex);
}

void Weather_SetBlendCoeffs(u8 eva, u8 evb)
{
    gWeatherPtr->currBlendEVA = eva;
    gWeatherPtr->currBlendEVB = evb;
    gWeatherPtr->targetBlendEVA = eva;
    gWeatherPtr->targetBlendEVB = evb;
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(eva, evb));
}

void Weather_SetTargetBlendCoeffs(u8 eva, u8 evb, int delay)
{
    gWeatherPtr->targetBlendEVA = eva;
    gWeatherPtr->targetBlendEVB = evb;
    gWeatherPtr->blendDelay = delay;
    gWeatherPtr->blendFrameCounter = 0;
    gWeatherPtr->blendUpdateCounter = 0;
}

bool8 Weather_UpdateBlend(void)
{
    if (gWeatherPtr->currBlendEVA == gWeatherPtr->targetBlendEVA
     && gWeatherPtr->currBlendEVB == gWeatherPtr->targetBlendEVB)
        return TRUE;

    if (++gWeatherPtr->blendFrameCounter > gWeatherPtr->blendDelay)
    {
        gWeatherPtr->blendFrameCounter = 0;
        gWeatherPtr->blendUpdateCounter++;

        // Update currBlendEVA and currBlendEVB on alternate frames
        if (gWeatherPtr->blendUpdateCounter & 1)
        {
            if (gWeatherPtr->currBlendEVA < gWeatherPtr->targetBlendEVA)
                gWeatherPtr->currBlendEVA++;
            else if (gWeatherPtr->currBlendEVA > gWeatherPtr->targetBlendEVA)
                gWeatherPtr->currBlendEVA--;
        }
        else
        {
            if (gWeatherPtr->currBlendEVB < gWeatherPtr->targetBlendEVB)
                gWeatherPtr->currBlendEVB++;
            else if (gWeatherPtr->currBlendEVB > gWeatherPtr->targetBlendEVB)
                gWeatherPtr->currBlendEVB--;
        }
    }

    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(gWeatherPtr->currBlendEVA, gWeatherPtr->currBlendEVB));

    if (gWeatherPtr->currBlendEVA == gWeatherPtr->targetBlendEVA
     && gWeatherPtr->currBlendEVB == gWeatherPtr->targetBlendEVB)
        return TRUE;

    return FALSE;
}

u8 GetCurrentWeather(void)
{
    return gWeatherPtr->currWeather;
}

bool8 IsThunderstorm(void)
{
    return gWeatherPtr->currWeather == WEATHER_RAIN && gWeatherPtr->currIntensity == WTHR_INTENSITY_EXTREME;
}

u16 GetRainSEFromIntensity(u8 intensity)
{
    switch (intensity)
    {
    default:
    case WTHR_INTENSITY_LOW:
        return SE_LIGHT_RAIN;
    case WTHR_INTENSITY_MILD:
        return SE_RAIN;
    case WTHR_INTENSITY_STRONG:
        return SE_DOWNPOUR;
    case WTHR_INTENSITY_EXTREME:
        return SE_THUNDERSTORM;
    }
}

void PlayRainSoundEffect(u16 se)
{
    if (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_SCREEN_FADING_OUT)
    {
        gWeatherPtr->rainSEPlaying = se;
        PlaySE(se);
    }
}

void PlayRainStoppingSoundEffect(void)
{
    if (IsSpecialSEPlaying())
    {
        // Rain stopping sound effects are always the next index after the standard rain sound effects
        PlaySE(gWeatherPtr->rainSEPlaying + 1);
        gWeatherPtr->rainSEPlaying = 0;
    }
}

u8 IsWeatherChangeComplete(void)
{
    return gWeatherPtr->weatherChangeComplete;
}

void SetWeatherScreenFadeOut(void)
{
    gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_SCREEN_FADING_OUT;
}

void SetWeatherPalStateIdle(void)
{
    gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;
}

void PreservePaletteInWeather(u8 preservedPalIndex)
{
    CpuCopy16(sBasePaletteColorMapTypes, sFieldEffectPaletteColorMapTypes, 32);
    sFieldEffectPaletteColorMapTypes[preservedPalIndex] = COLOR_MAP_NONE;
    sPaletteColorMapTypes = sFieldEffectPaletteColorMapTypes;
}

void ResetPreservedPalettesInWeather(void)
{
    sPaletteColorMapTypes = sBasePaletteColorMapTypes;
}
