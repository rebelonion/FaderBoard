#include "FaderMotor.h"

FaderMotor::FaderMotor(const uint8_t _forwardPin, const uint8_t _backwardPin) {
    forwardPin = _forwardPin;
    backwardPin = _backwardPin;
    pinMode(forwardPin, OUTPUT);
    pinMode(backwardPin, OUTPUT);
    //get rid of the motor noise
    analogWriteFrequency(forwardPin, 25000);
    analogWriteFrequency(backwardPin, 25000);
    analogWrite(forwardPin, 0);
    analogWrite(backwardPin, 0);
}

FaderMotor::~FaderMotor() = default;

void FaderMotor::forward(uint8_t speedPercentage) const {
    if (speedPercentage > 100) {
        speedPercentage = 100;
    }
    if (speedPercentage < 0) {
        speedPercentage = 0;
    }
    const uint8_t mappedSpeed = map(speedPercentage, 0, 100, 0, 255);
    analogWrite(backwardPin, 0);
    analogWrite(forwardPin, mappedSpeed);
}

void FaderMotor::backward(uint8_t speedPercentage) const {
    if (speedPercentage > 100) {
        speedPercentage = 100;
    }
    if (speedPercentage < 0) {
        speedPercentage = 0;
    }
    const uint8_t mappedSpeed = map(speedPercentage, 0, 100, 0, 255);
    analogWrite(forwardPin, 0);
    analogWrite(backwardPin, mappedSpeed);
}

void FaderMotor::stop() const {
    analogWrite(forwardPin, 0);
    analogWrite(backwardPin, 0);
}
