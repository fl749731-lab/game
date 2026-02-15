#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "game/farming.h"  // Season

#include <string>
#include <functional>
#include <vector>

namespace Engine {

// ── 天气 ──────────────────────────────────────────────────

enum class Weather : u8 {
    Sunny = 0,
    Rainy,
    Stormy,
    Snowy,
};

inline const char* WeatherName(Weather w) {
    switch (w) {
        case Weather::Sunny:  return "晴";
        case Weather::Rainy:  return "雨";
        case Weather::Stormy: return "暴风雨";
        case Weather::Snowy:  return "雪";
        default: return "?";
    }
}

// ── 时间事件 ──────────────────────────────────────────────

enum class TimeEvent : u8 {
    NewHour = 0,
    NewDay,
    NewSeason,
    NewYear,
    Sleeping,
};

using TimeCallback = std::function<void(TimeEvent)>;

// ── 游戏时间系统 ──────────────────────────────────────────

class GameTimeSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "GameTimeSystem"; }

    u32     GetHour()    const { return m_Hour; }
    u32     GetMinute()  const { return m_Minute; }
    u32     GetDay()     const { return m_Day; }
    Season  GetSeason()  const { return m_Season; }
    u32     GetYear()    const { return m_Year; }
    Weather GetWeather() const { return m_Weather; }

    std::string GetTimeString() const;
    std::string GetDateString() const;
    bool IsNight() const { return m_Hour >= 18 || m_Hour < 6; }
    f32 GetDaylightFactor() const;

    void SetTimeScale(f32 minsPerSec) { m_TimeScale = minsPerSec; }
    void SetDaysPerSeason(u32 d) { m_DaysPerSeason = d; }
    void SetTime(u32 hour, u32 minute);
    void SetDate(u32 day, Season season, u32 year);
    void Sleep();
    void Pause()  { m_Paused = true; }
    void Resume() { m_Paused = false; }
    bool IsPaused() const { return m_Paused; }
    void SetWeather(Weather w) { m_Weather = w; }
    void OnTimeEvent(TimeCallback cb) { m_Callbacks.push_back(cb); }

private:
    void Fire(TimeEvent e);
    void RandomizeWeather();

    f32     m_TimeScale     = 7.0f;
    u32     m_DaysPerSeason = 28;
    u32     m_Hour   = 6;
    u32     m_Minute = 0;
    f32     m_Accumulator = 0.0f;
    u32     m_Day    = 1;
    Season  m_Season = Season::Spring;
    u32     m_Year   = 1;
    Weather m_Weather = Weather::Sunny;
    bool    m_Paused  = false;
    std::vector<TimeCallback> m_Callbacks;
};

} // namespace Engine
