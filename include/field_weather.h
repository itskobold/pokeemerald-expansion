#ifndef GUARD_WEATHER_H
#define GUARD_WEATHER_H

#include "sprite.h"
#include "constants/field_weather.h"

#define TAG_WEATHER_START 0x1200
enum {
    GFXTAG_CLOUD = TAG_WEATHER_START,
    GFXTAG_FOG_H,
    GFXTAG_ASH,
    GFXTAG_FOG_D,
    GFXTAG_SANDSTORM,
    GFXTAG_BUBBLE,
    GFXTAG_RAIN,
};
enum {
    PALTAG_WEATHER = TAG_WEATHER_START,
    PALTAG_WEATHER_2
};

#define NUM_WEATHER_COLOR_MAPS 19

struct Weather
{
    union
    {
        struct
        {
            struct Sprite *rainSprites[MAX_RAIN_SPRITES];
            struct Sprite *snowflakeSprites[101];
            struct Sprite *cloudSprites[NUM_CLOUD_SPRITES];
        } s1;
        struct
        {
            u8 filler0[0xA0];
            struct Sprite *fogHSprites[NUM_FOG_HORIZONTAL_SPRITES];
            struct Sprite *ashSprites[NUM_ASH_SPRITES];
            struct Sprite *fogDSprites[NUM_FOG_DIAGONAL_SPRITES];
            struct Sprite *sandstormSprites1[NUM_SANDSTORM_SPRITES];
            struct Sprite *sandstormSprites2[NUM_SWIRL_SANDSTORM_SPRITES];
        } s2;
    } sprites;
    u8 darkenedContrastColorMaps[NUM_WEATHER_COLOR_MAPS][32];
    u8 contrastColorMaps[NUM_WEATHER_COLOR_MAPS][32];
    s8 colorMapIndex;
    s8 targetColorMapIndex;
    u8 colorMapStepDelay;
    u8 colorMapStepCounter;
    u16 fadeDestColor;
    u8 palProcessingState;
    u8 fadeScreenCounter;
    bool8 readyForInit;
    u8 taskId;
    u8 fadeInFirstFrame;
    u8 fadeInTimer;
    u16 initStep;
    u16 finishStep;
    u8 currWeather;
    u8 nextWeather;
    u8 currIntensity;
    u8 nextIntensity;
    u8 weatherGfxLoaded;
    bool8 weatherChangeComplete;
    u8 weatherPicSpritePalIndex;
    u8 contrastColorMapSpritePalIndex;
    u8 nextAbnormalWeather;
    u8 cloudSpritesCreated;
    // Sunny
    // Rain
    u8 rainStep;
    u16 rainSpriteVisibleCounter;
    u8 curRainSpriteIndex;
    u8 targetRainSpriteCount;
    u8 rainSpriteCount;
    u8 rainSpriteVisibleDelay;
    bool8 updatingRainSprites;
    u8 isDownpour;
    u16 rainSEPlaying;
    u16 thunderTimer;             // general-purpose timer for state transitions
    u16 thunderSETimer;           // timer for thunder sound effect
    bool8 thunderAllowEnd;
    bool8 thunderLongBolt;        // true if this cycle will end in a long lightning bolt
    u8 thunderShortBolts;         // the number of short bolts this cycle
    bool8 thunderEnqueued;
    // Snow
    u16 snowflakeVisibleCounter;
    u16 snowflakeTimer;
    u8 snowflakeSpriteCount;
    u8 targetSnowflakeSpriteCount;
    // Horizontal fog
    u16 fogHScrollPosX;
    u16 fogHScrollCounter;
    u16 fogHScrollOffset;
    u8 lightenedFogSpritePals[6];
    u8 lightenedFogSpritePalsCount;
    u8 fogHSpritesCreated;
    // Ash
    u16 ashBaseSpritesX;
    u16 ashUnused;
    u8 ashSpritesCreated;
    // Sandstorm
    u32 sandstormXOffset;
    u32 sandstormYOffset;
    u16 sandstormUnused;
    u16 sandstormBaseSpritesX;
    u16 sandstormPosY;
    u16 sandstormWaveIndex;
    u16 sandstormWaveCounter;
    u8 sandstormSpritesCreated;
    u8 sandstormSwirlSpritesCreated;
    // Diagonal fog
    u16 fogDBaseSpritesX;
    u16 fogDPosY;
    u16 fogDScrollXCounter;
    u16 fogDScrollYCounter;
    u16 fogDXOffset;
    u16 fogDYOffset;
    u8 fogDSpritesCreated;
    // Bubbles
    u16 bubblesDelayCounter;
    u16 bubblesDelayIndex;
    u16 bubblesCoordsIndex;
    u16 bubblesSpriteCount;
    u8 bubblesSpritesCreated;

    u16 currBlendEVA;
    u16 currBlendEVB;
    u16 targetBlendEVA;
    u16 targetBlendEVB;
    u8 blendUpdateCounter;
    u8 blendFrameCounter;
    u8 blendDelay;
};

// field_weather.c
extern struct Weather gWeather;
extern struct Weather *const gWeatherPtr;
extern const u16 gFogPalette[];

// field_weather_effect.c
extern const u8 gWeatherFogHorizontalTiles[];

void StartWeather(void);
void SetNextWeather(u8 weather);
void SetCurrentAndNextWeather(u8 weather);
void SetCurrentAndNextWeatherNoDelay(u8 weather);
void SetNextWeatherIntensity(u8 intensity);
void DoCoordEventWeather(u8 weather);
void DoCoordEventWeatherIntensity(u8 intensity);
void SetCurrentAndNextWeatherIntensity(u8 intensity);
void ApplyWeatherColorMapIfIdle(s8 colorMapIndex);
void ApplyWeatherColorMapIfIdle_Gradual(u8 colorMapIndex, u8 targetColorMapIndex, u8 colorMapStepDelay);
void FadeScreen(u8 mode, s8 delay);
bool8 IsWeatherNotFadingIn(void);
void UpdateSpritePaletteWithWeather(u8 spritePaletteIndex);
void ApplyWeatherColorMapToPal(u8 paletteIndex);
void LoadCustomWeatherSpritePalette(const u16 *palette);
void Weather_SetBlendCoeffs(u8 eva, u8 evb);
void Weather_SetTargetBlendCoeffs(u8 eva, u8 evb, int delay);
bool8 Weather_UpdateBlend(void);
u8 GetCurrentWeather(void);
bool8 IsThunderstorm(void);
u16 GetRainSEFromIntensity(u8 intensity);
void PlayRainSoundEffect(u16 se);
void PlayRainStoppingSoundEffect(void);
u8 IsWeatherChangeComplete(void);
void SetWeatherScreenFadeOut(void);
void SetWeatherPalStateIdle(void);
void PreservePaletteInWeather(u8 preservedPalIndex);
void ResetPreservedPalettesInWeather(void);

// field_weather_effect.c
void Clouds_InitVars(void);
void Clouds_Main(void);
void Clouds_InitAll(void);
bool8 Clouds_Finish(void);
void Sunny_InitVars(void);
void Sunny_InitAll(void);
void Sunny_Intensity(void);
void Sunny_Main(void);
bool8 Sunny_Finish(void);
void Normal_InitVars(void);
void Normal_Main(void);
void Normal_InitAll(void);
void Normal_Intensity(void);
bool8 Normal_Finish(void);
void Rain_InitVars(void);
void Rain_Intensity(void);
void Rain_Main(void);
void Rain_InitAll(void);
bool8 Rain_Finish(void);
void Snow_InitVars(void);
void Snow_Main(void);
void Snow_InitAll(void);
bool8 Snow_Finish(void);
void FogHorizontal_InitVars(void);
void FogHorizontal_Main(void);
void FogHorizontal_InitAll(void);
bool8 FogHorizontal_Finish(void);
void Ash_InitVars(void);
void Ash_Main(void);
void Ash_InitAll(void);
bool8 Ash_Finish(void);
void Sandstorm_InitVars(void);
void Sandstorm_Main(void);
void Sandstorm_InitAll(void);
bool8 Sandstorm_Finish(void);
void FogDiagonal_InitVars(void);
void FogDiagonal_Main(void);
void FogDiagonal_InitAll(void);
bool8 FogDiagonal_Finish(void);
void Bubbles_InitVars(void);
void Bubbles_Main(void);
void Bubbles_InitAll(void);
bool8 Bubbles_Finish(void);

u8 GetSavedWeather(void);
u8 GetSavedWeatherIntensity(void);
u8 GetCurrentAbnormalWeatherIntensity(void);
void SetSavedWeather(u8 weather);
void SetSavedWeatherIntensity(u8 intensity);
void SetSavedWeatherFromCurrMapHeader(void);
void SetWeather(u8 weather);
void SetWeatherIntensity(u8 intensity);
void DoCurrentWeather(void);
void ResumePausedWeather(void);

#endif // GUARD_WEATHER_H
