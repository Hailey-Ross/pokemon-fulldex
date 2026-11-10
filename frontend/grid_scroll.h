#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Drag directly under the pointer; coast only after release. Wheel impulses share the
// same bounded motion, without moving controller focus or quantising to whole rows.
class GridScrollMotion
{
    double position = 0, velocity = 0;
    uint32_t tick = 0, lastMove = 0;
    bool active = false;
public:
    bool Active() const { return active; }
    void Stop() { active = false; velocity = 0; }
    void Begin(int value, uint32_t now) { Stop(); position = value; tick = lastMove = now; }
    void Drag(int value, uint32_t now)
    {
        const uint32_t elapsed = now - tick;
        if (elapsed && elapsed <= 100)
            velocity = std::clamp(0.5 * velocity + 0.5 * (value - position) * 1000.0 / elapsed, -5000.0, 5000.0);
        else if (elapsed > 100) velocity = 0;
        if (position != value) lastMove = now;
        position = value; tick = now;
    }
    void End(uint32_t now)
    {
        if (now - lastMove > 100) velocity = 0;
        active = std::abs(velocity) > 30; tick = now;
    }
    void Wheel(double pixels, int value, uint32_t now)
    {
        if (!active) position = value;
        velocity = std::clamp(velocity + pixels * 10, -5000.0, 5000.0);
        active = true; tick = now;
    }
    void Advance(uint32_t now, int maximum, int& value)
    {
        if (!active) return;
        const uint32_t elapsed = now - tick; tick = now;
        if (elapsed > 250) { Stop(); return; } // Backgrounding must not cause a large jump.
        const double dt = std::min(elapsed, uint32_t(50)) / 1000.0;
        const double decay = std::exp(-10 * dt);
        position += velocity * (1 - decay) / 10;
        velocity *= decay;
        position = std::clamp(position, 0.0, (double)std::max(0, maximum));
        value = (int)std::lround(position);
        if (position <= 0 || position >= maximum || std::abs(velocity) < 5) Stop();
    }
};
