
#ifndef __FADERMOTOR_H__
#define __FADERMOTOR_H__

#include <Arduino.h>

class FaderMotor
{
private:
    int forwardPin;
    int backwardPin;
public:
    FaderMotor(int _forwardPin, int _backwardPin);
    ~FaderMotor();
    void forward(int speedPercentage);
    void backward(int speedPercentage);
    void stop();
};

FaderMotor::FaderMotor(int _forwardPin, int _backwardPin)
{
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

FaderMotor::~FaderMotor()
{
}

void FaderMotor::forward(int speedPercentage)
{
    if(speedPercentage > 100)
    {
        speedPercentage = 100;
    }
    if(speedPercentage < 0)
    {
        speedPercentage = 0;
    }
    int mappedSpeed = map(speedPercentage, 0, 100, 0, 255);
    analogWrite(backwardPin, 0);
    analogWrite(forwardPin, mappedSpeed);
}

void FaderMotor::backward(int speedPercentage)
{
    if(speedPercentage > 100)
    {
        speedPercentage = 100;
    }
    if(speedPercentage < 0)
    {
        speedPercentage = 0;
    }
    int mappedSpeed = map(speedPercentage, 0, 100, 0, 255);
    analogWrite(forwardPin, 0);
    analogWrite(backwardPin, mappedSpeed);
}

void FaderMotor::stop()
{
    analogWrite(forwardPin, 0);
    analogWrite(backwardPin, 0);
}


#endif