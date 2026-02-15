#include "game/time_system.h"
#include "engine/core/log.h"

#include <cmath>
#include <cstdlib>

namespace Engine {

void GameTimeSystem::Update(ECSWorld& world, f32 dt) {
    (void)world;
    if (m_Paused) return;

    m_Accumulator += dt * m_TimeScale;
    while (m_Accumulator >= 1.0f) {
        m_Accumulator -= 1.0f;
        m_Minute++;
        if (m_Minute >= 60) {
            m_Minute = 0;
            m_Hour++;
            Fire(TimeEvent::NewHour);
            if (m_Hour >= 24) {
                m_Hour = 0;
                m_Day++;
                RandomizeWeather();
                Fire(TimeEvent::NewDay);
                if (m_Day > m_DaysPerSeason) {
                    m_Day = 1;
                    u8 s = (u8)m_Season + 1;
                    if (s >= (u8)Season::COUNT) { s = 0; m_Year++; Fire(TimeEvent::NewYear); }
                    m_Season = (Season)s;
                    Fire(TimeEvent::NewSeason);
                }
            }
        }
    }
}

std::string GameTimeSystem::GetTimeString() const {
    bool pm = m_Hour >= 12;
    u32 h12 = m_Hour % 12;
    if (h12 == 0) h12 = 12;
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %u:%02u", pm ? "下午" : "上午", h12, m_Minute);
    return buf;
}

std::string GameTimeSystem::GetDateString() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s 第%u天 第%u年", SeasonName(m_Season), m_Day, m_Year);
    return buf;
}

f32 GameTimeSystem::GetDaylightFactor() const {
    f32 t = (f32)m_Hour + (f32)m_Minute / 60.0f;
    if (t >= 6.0f && t <= 18.0f) return 1.0f;
    else if (t > 18.0f && t <= 21.0f) return 1.0f - 0.7f * ((t - 18.0f) / 3.0f);
    else if (t > 21.0f) return 0.3f;
    else if (t < 4.0f) return 0.2f;
    else return 0.2f + 0.8f * ((t - 4.0f) / 2.0f);
}

void GameTimeSystem::SetTime(u32 hour, u32 minute) {
    m_Hour = hour; m_Minute = minute; m_Accumulator = 0.0f;
}

void GameTimeSystem::SetDate(u32 day, Season season, u32 year) {
    m_Day = day; m_Season = season; m_Year = year;
}

void GameTimeSystem::Sleep() {
    m_Paused = true;
    m_Hour = 6; m_Minute = 0; m_Accumulator = 0.0f;
    m_Day++;
    if (m_Day > m_DaysPerSeason) {
        m_Day = 1;
        u8 s = (u8)m_Season + 1;
        if (s >= (u8)Season::COUNT) { s = 0; m_Year++; Fire(TimeEvent::NewYear); }
        m_Season = (Season)s;
        Fire(TimeEvent::NewSeason);
    }
    RandomizeWeather();
    Fire(TimeEvent::Sleeping);
    Fire(TimeEvent::NewDay);
    m_Paused = false;
}

void GameTimeSystem::Fire(TimeEvent e) {
    for (auto& cb : m_Callbacks) cb(e);
}

void GameTimeSystem::RandomizeWeather() {
    u32 r = (u32)rand() % 100;
    if (m_Season == Season::Winter) {
        if (r < 50) m_Weather = Weather::Sunny;
        else if (r < 80) m_Weather = Weather::Snowy;
        else m_Weather = Weather::Stormy;
    } else {
        if (r < 70) m_Weather = Weather::Sunny;
        else if (r < 90) m_Weather = Weather::Rainy;
        else m_Weather = Weather::Stormy;
    }
}

} // namespace Engine
