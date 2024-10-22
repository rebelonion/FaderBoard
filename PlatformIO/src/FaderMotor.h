#pragma once

#include <Arduino.h>

class FaderMotor {
public:
    FaderMotor(uint8_t _forwardPin, uint8_t _backwardPin);
    ~FaderMotor();
    void forward(uint8_t speedPercentage) const;
    void backward(uint8_t speedPercentage) const;
    void stop() const;

private:
    uint8_t forwardPin;
    uint8_t backwardPin;
};
