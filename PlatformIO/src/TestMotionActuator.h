// RBotFirmware
// Rob Dobson 2016-2018

#pragma once

//#define USE_FAST_PIN_ACCESS 1
//#define USE_IRQ_CONTROL 1

// Comment out these items to enable/disable test code
#ifdef SPARK
#define TEST_MOTION_ACTUATOR_ENABLE    1
#define TEST_MOTION_ACTUATOR_OUTPUT    1
#define SystemTicksPerMicrosecond      System.ticksPerMicrosecond()
#define SystemTicks                    System.ticks()
#else
//#define TEST_MOTION_ACTUATOR_ENABLE    1
//#define TEST_MOTION_ACTUATOR_OUTPUT    1
#include "xtensa/core-macros.h"
#define SystemTicksPerMicrosecond      XTHAL_GET_CCOUNT()
#define SystemTicks                    (10000000)
#endif
#define TEST_MOTION_ACTUATOR_CONFIG "TIMEISR BLINKD7"

#include <ArduinoLog.h>
#include "MotionRingBuffer.h"
#include "ConfigPinMap.h"
#include <vector>

static constexpr int TEST_OUTPUT_STEPS = 1000;

#ifdef TEST_MOTION_ACTUATOR_OUTPUT
#define TEST_MOTION_ACTUATOR_PROCESS      if (_pTestMotionActuator) _pTestMotionActuator->process();
#define TEST_MOTION_ACTUATOR_STEP_END     if (_pTestMotionActuator) _pTestMotionActuator->stepEnd();
#define TEST_MOTION_ACTUATOR_STEP_DIRN    if (_pTestMotionActuator) _pTestMotionActuator->stepDirn(axisIdx, stepsTotal >= 0);
#define TEST_MOTION_ACTUATOR_STEP_START(AX_IDX)    if (_pTestMotionActuator) _pTestMotionActuator->stepStart(AX_IDX);
#else
#define TEST_MOTION_ACTUATOR_PROCESS
#define TEST_MOTION_ACTUATOR_STEP_END
#define TEST_MOTION_ACTUATOR_STEP_DIRN
#define TEST_MOTION_ACTUATOR_STEP_START(AX_IDX)
#endif

#ifndef TEST_MOTION_ACTUATOR_ENABLE

#define TEST_MOTION_ACTUATOR_DEF
#define TEST_MOTION_ACTUATOR_TIME_START
#define TEST_MOTION_ACTUATOR_TIME_END

#else

#define TEST_MOTION_ACTUATOR_DEF           TestMotionActuator * MotionActuator::_pTestMotionActuator = NULL;
#define TEST_MOTION_ACTUATOR_TIME_START    if (_pTestMotionActuator) { _pTestMotionActuator->blink(); _pTestMotionActuator->timeStart(); }
#define TEST_MOTION_ACTUATOR_TIME_END      if (_pTestMotionActuator) _pTestMotionActuator->timeEnd();

class TestOutputStepData
{
public:
  struct TestOutputStepInf
  {
    uint32_t _micros;
    struct
    {
      int _pin : 8;
      int _val : 1;
    };
  };
  MotionRingBufferPosn _stepBufPos;
  std::vector<TestOutputStepInf> _stepBuf;

  TestOutputStepData() :
    _stepBufPos(TEST_OUTPUT_STEPS)
  {
    _stepBuf.resize(TEST_OUTPUT_STEPS);
  }

  void stepStart(int axisIdx)
  {
    addToQueue(axisIdx == 0 ? 2 : 4, 1);
  }

  void stepEnd()
  {
  }

  void stepDirn(int axisIdx, int val)
  {
    addToQueue(axisIdx == 0 ? 3 : 5, val);
  }

  void addToQueue(int pin, int val)
  {
    // Ignore if it is a lowering of a step pin (to avoid end of test problem)
    if ((val == 0) && (pin == 17 || pin == 15))
      return;
    if (_stepBufPos.canPut())
    {
      TestOutputStepInf newInf;
      newInf._micros                = micros();
      newInf._pin                   = uint8_t(pin);
      newInf._val                   = val;
      _stepBuf[_stepBufPos._putPos] = newInf;
      _stepBufPos.hasPut();
    }
  }

  TestOutputStepInf getStepInf()
  {
    TestOutputStepInf inf = _stepBuf[_stepBufPos._getPos];
    _stepBufPos.hasGot();
    return inf;
  }

  void process()
  {
    // Log.trace("StepBuf getPos %d putPos %d count %d", _stepBufPos._getPos, _stepBufPos._putPos, _stepBufPos.count());

    // Get
    for (int i = 0; i < 5; i++)
    {
      // Check if can get
      if (!_stepBufPos.canGet())
      {
        // Log.trace("Process can't get");
        return;
      }

      //TestOutputStepInf inf = getStepInf();
      //Log.trace("W\t%lu\t%d\t%d", inf._micros, inf._pin, inf._val ? 1 : 0);
    }
  }
};
#endif

class TestMotionActuator
{
public:
  bool _outputStepData;
  bool _blinkD7OnISR;
  bool _timeISR;
  int _pinNum;
  uint32_t __isrDbgTickMin;
  uint32_t __isrDbgTickMax;
  uint32_t __isrDbgTickCount;
  uint32_t __isrDbgTickSum;
  uint32_t __startTicks;

  TestMotionActuator()
  {
    _blinkD7OnISR     = false;
    _outputStepData   = false;
    _timeISR          = false;
    __isrDbgTickMin   = 100000000;
    __isrDbgTickMax   = 0;
    __isrDbgTickCount = 0;
    __isrDbgTickSum   = 0;
  }
  ~TestMotionActuator()
  {
  }
#ifdef TEST_MOTION_ACTUATOR_ENABLE
  TestOutputStepData _testOutputStepData;
#endif
  static uint32_t _testCount;

  void setTestMode(const char* pTestModeStr)
  {
    _blinkD7OnISR = false;
    if (strstr(pTestModeStr, "BLINKD7") != NULL)
      _blinkD7OnISR = true;
    _outputStepData = false;
    if (strstr(pTestModeStr, "OUTPUTSTEPDATA") != NULL)
      _outputStepData = true;
    _timeISR = false;
    if (strstr(pTestModeStr, "TIMEISR") != NULL)
      _timeISR = true;
    Log.notice("TestMotionActuator: blink %d, outputStepData %d, timeISR %d",
             _blinkD7OnISR, _outputStepData, _timeISR);
#ifndef TEST_MOTION_ACTUATOR_OUTPUT
    if (_outputStepData)
    {
      Log.notice(" ");
      Log.notice("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Log.notice("OUTPUTSTEPDATA is selected BUT TEST_MOTION_ACTUATOR_OUTPUT in not defined");
      Log.notice(" ");
    }
#endif
    if (_blinkD7OnISR)
    {
      _pinNum = ConfigPinMap::getPinFromName("D7");
      pinMode(_pinNum, OUTPUT);
    }
    __isrDbgTickMin   = 100000000;
    __isrDbgTickMax   = 0;
    __isrDbgTickCount = 0;
    __isrDbgTickSum   = 0;
  }

  void process()
  {
#ifdef TEST_MOTION_ACTUATOR_ENABLE
    if (_outputStepData)
      _testOutputStepData.process();
#endif
  }

  void blink()
  {
    uint32_t blinkRate = 10000;
    _testCount++;
    if (_testCount > blinkRate)
    {
      _testCount = 0;
      #ifdef USE_FAST_PIN_ACCESS
        digitalWriteFast(_pinNum, !pinReadFast(_pinNum));
      #else
        digitalWrite(_pinNum, !digitalRead(_pinNum));
      #endif
    }
  }

  inline void timeStart()
  {
    if (_timeISR)
    {
      #ifdef USE_IRQ_CONTROL
        __disable_irq();
      #endif
      __startTicks = (SystemTicks);
    }
  }

  inline void timeEnd()
  {
    if (_timeISR)
    {
      // Time the ISR over entire execution
      uint32_t endTicks = (SystemTicks);
      #ifdef USE_IRQ_CONTROL
        __enable_irq();
      #endif
      uint32_t elapsedTicks = endTicks - __startTicks;
      if (__startTicks < endTicks)
        elapsedTicks = 0xffffffff - __startTicks + endTicks;
      if (__isrDbgTickMin > elapsedTicks)
        __isrDbgTickMin = elapsedTicks;
      if (__isrDbgTickMax < elapsedTicks)
        __isrDbgTickMax = elapsedTicks;
      __isrDbgTickCount++;
      __isrDbgTickSum += elapsedTicks;
    }
  }

  inline void stepStart(int axisIdx)
  {
#ifdef TEST_MOTION_ACTUATOR_ENABLE
    if (_outputStepData)
      _testOutputStepData.stepStart(axisIdx);
#endif
  }

  inline void stepEnd()
  {
#ifdef TEST_MOTION_ACTUATOR_ENABLE
    if (_outputStepData)
      _testOutputStepData.stepEnd();
#endif
  }

  inline void stepDirn(int axisIdx, int val)
  {
#ifdef TEST_MOTION_ACTUATOR_ENABLE
    if (_outputStepData)
      _testOutputStepData.stepDirn(axisIdx, val);
#endif
  }

  String getDebugStr()
  {
    char strBuf[200];
    sprintf(strBuf, "ISR Mn/Mx/Av/# %0.2fuS/%0.2fuS/%0.2fus/%ld",
             ((double) __isrDbgTickMin) / SystemTicksPerMicrosecond,
             ((double) __isrDbgTickMax) / SystemTicksPerMicrosecond,
             (__isrDbgTickCount != 0) ?
             ((double) (__isrDbgTickSum * 1.0 / __isrDbgTickCount) / SystemTicksPerMicrosecond):
             0,
             __isrDbgTickCount);
    __isrDbgTickMin   = 10000000;
    __isrDbgTickMax   = 0;
    __isrDbgTickCount = 0;
    __isrDbgTickSum   = 0;
    return strBuf;
  }

  void showDebug()
  {
#ifdef TEST_MOTION_ACTUATOR_ENABLE
    Log.notice(getDebugStr().c_str());
#endif
  }
};
