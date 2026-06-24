#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <cstdint>

static std::vector<std::array<uint8_t, 8>> generateSimulationData() {
    const int fps = 60;
    const int durationSeconds = 600; // 10 Minuten
    const int totalFrames = fps * durationSeconds;

    std::vector<std::array<uint8_t, 8>> simData;
    simData.reserve(totalFrames);

    float currentSpeed = 0.0f;
    float currentRpm = 800.0f; // Start im Leerlauf
    float fuel = 100.0f;       // 100% Tank
    float temp = 20.0f;        // 20°C Kaltstart

    for (int i = 0; i < totalFrames; ++i) {
        float timeS = static_cast<float>(i) / fps;

        // 1. Fahrprofil (Einfache Beschleunigungs- und Bremsphasen)
        if (timeS > 10.0f && timeS < 100.0f) {
            if (currentSpeed < 50.0f) currentSpeed += 0.015f; // Stadtverkehr
        } else if (timeS > 150.0f && timeS < 400.0f) {
            if (currentSpeed < 100.0f) currentSpeed += 0.02f; // Landstraße
        } else if (timeS > 500.0f) {
            if (currentSpeed > 0.0f) currentSpeed -= 0.04f;   // Abbremsen zum Ende
        }
        if (currentSpeed < 0.0f) currentSpeed = 0.0f;

        // 2. Drehzahl-Berechnung (gekoppelt an Geschwindigkeit + "virtuelle Gänge")
        int gear = (static_cast<int>(currentSpeed) / 25) + 1;
        if (currentSpeed < 1.0f) {
            currentRpm = 800.0f;
        } else {
            // RPM steigt bis zum Schaltvorgang an und fällt dann leicht ab
            currentRpm = 1200.0f + (currentSpeed - ((gear - 1) * 25.0f)) * 120.0f;
        }

        // 3. Temperatur & Tank (Langsame Änderungen)
        if (temp < 90.0f) temp += 0.003f; // Aufwärmen bis 90 Grad
        fuel -= 0.0002f;                  // Leichter Benzinverbrauch

        // 4. Blinker-Logik (z.B. Blinken vor dem Losfahren und beim Abbiegen)
        uint8_t leftInd = 0;
        uint8_t rightInd = 0;
        // Blinker Links bei Sekunde 8-12 und Sekunde 145-150
        if ((timeS >= 8.0f && timeS <= 12.0f) || (timeS >= 145.0f && timeS <= 150.0f)) {
            // Modulo-Rechnung für den Blinkertakt (~1 Hz)
            leftInd = (static_cast<int>(timeS * 1.5f) % 2 == 0) ? 1 : 0;
        }

        // 5. RPM in zwei Bytes splitten
        uint16_t rpmInt = static_cast<uint16_t>(currentRpm);
        uint8_t upperRpmByte = (rpmInt >> 8) & 0xFF;
        uint8_t lowerRpmByte = rpmInt & 0xFF;

        // 6. Frame in Vector pushen
        simData.push_back({
            static_cast<uint8_t>(fuel),
            1, // Platzhalter
            static_cast<uint8_t>(temp),
            upperRpmByte,
            lowerRpmByte,
            static_cast<uint8_t>(currentSpeed),
            leftInd,
            rightInd
        });
    }

    return simData;
}