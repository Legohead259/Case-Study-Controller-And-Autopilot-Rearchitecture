# Handling Non-Critical Timing Events

## Original Code

```cpp
// Event 1
if (millis()-heartbeattimer>2000) { // Send a heartbeat message every 2 seconds
    LoRa.beginPacket();
    LoRa.print("HB");
    LoRa.endPacket();
    heartbeattimer=millis();
    Serial.println("Sending HB");
}
.
.
.
// Event 2
if (xvalue!=t_xvalue && millis()-XsendingTimer>200) { // Send a new joystick value every 200 ms, but only if its changed from the last reading
    xvalue=t_xvalue;
    LoRa.beginPacket();
    LoRa.print(xvalue);
    LoRa.endPacket();
    Serial.print("X: ");
    Serial.println(xvalue);
    XsendingTimer=millis();
}

// Event 3
if (yvalue!=t_yvalue && millis()-XsendingTimer>300) { // Send a new joystick value every 300 ms, but only if its changed from the last reading
    yvalue=t_yvalue;
    LoRa.beginPacket();
    LoRa.print(yvalue);
    LoRa.endPacket();
    Serial.print("Y: ");
    Serial.println(yvalue);
    YsendingTimer=millis();
}
```

## Problem

The common logic implementation for running a non-critical non-blocking (without using `delay()`) timing task in Arduino (like blinking an LED) is to check the current system time using `millis()`, finding the length of time since the task was last performed, `millis() - lastTimePerformed`, and then compare this difference to a timeout value, `ACTION_TIME`.
The entire `if-statement` looks like this:

```cpp
if (millis() - lastTimePerformed > ACTION_TIME) {
    // Perform action
    lastTimePerformed = millis();
}
```

Despite being logical, this requires two computations to be performed every loop.
One to subtract the `lastTimePerformed` from the `millis()` value, and another to do a comparison to `TASK_PERIOD`.
Neither are computationally expense, but the former takes many more compute cycles to execute because it handles `uint32` values.
This issue also increases linearly with the number of tasks.
If there are hundreds of non-time critical tasks to be performed, the microcontroller will compute hundreds of subtractions every loop just to make a simple boolean comparison.

Additionally, This method is poor object-oriented practice because for every new task, the same `if-statement` block would have to be added into the main execution loop.
If a future maintainer wants to tweak the parameters of a task, they would have to first track down the specific `if` block and adjust it manually.

## Solution

To solve this problem, we will take a three-step approach.
First, we will tweak the execution determination logic to only calculate the next execution time
Second, we shall define a task in an object-oriented manner where the task and its parameters are encapsulated into a `TimerEvent` task.
This object will contain all of the information needed to execute the task at a specific time, enable/disable its execution, and have callback functions for when the event occurs.
Third, we will define a `TimerEventManager` object that can build an array of `TimerEvent`s that it will iterate through every loop and determine if the event needs to be executed and then do so.

### Step One: Tweaking the Logic

To solve the first problem of the timer events scenario, we shall change the `if-statement` logic to only compare the `nextTimeToBePerformed` value to the current timestamp.
This `nextTime` value is calculated at the conclusion of the task after the `TASK_PERIOD` has passed since the last execution.
The tweaked `if-statement` block looks like the following:

```cpp
uint32_t currentTime = millis();
if (currentTime >= nextTimeToBePerformed) {
    // Do event
    nextTimeToBePerformed = currentTime + TASK_PERIOD;
}
```

With this logic, we cut the number of computations by half while still retaining the same functionality as the original code.
Because of how unsigned integers behave, this method is also rollover transparent for when the `millis()` function exceeds its maximum value and rolls over near 0.

### Step Two: Encapsulating the Task

We define a non-critical timing task as an event that needs to occur with a certain `period`, can have a certain `duration`, can be `enabled` or `disabled`, and can perform different functions on `execution` and `finish`.
We can also give these tasks a name that makes them more human-readable and easier to maintain/debug in the application.

This task can be defined in a class, [`TimerEvent`](https://github.com/Legohead259/Timer-Events-Arduino/blob/main/src/TimerEvents.h), that handles all these basic functions.

We can declare a basic task like blinking the on-board LED by implementing the following:

```cpp
TimerEvent blinkEvent(  "Blink Event",  // Name of the event
                        1000,           // Period, ms. Valid [0, MAX_VALUE_UINT32]
                        ledCallback,    // On execution callback function 
                        true,           // Enabled
                        -1,             // Duration, ms. Valid [0, MAX_VALUE_LONG]. Default: -1
                        nullptr)        // On finish callback function. Default: nullptr
```

### Step Three: Managing `TimerEvent`s

If we want to declare and handle multiple events, we will need a helper object to help us manage them.
This manager must be able to build and maintain an array of pointers to `events` and handle their execution properly.
Some events may have a duration they need to be active before stopping (e.g. blink an LED purple and white rapidly for 5 seconds).
Some events may also need to trigger another function when they end (e.g. resetting a watch dog or cleaning up memory after an action).

This manager can be found in a class declared in [`TimerEvent.h`](https://github.com/Legohead259/Timer-Events-Arduino/blob/main/src/TimerEvents.h) and defined in [`TimerEvents.cpp`](https://github.com/Legohead259/Timer-Events-Arduino/blob/main/src/TimerEvents.cpp).

We can declare and implement this task manager in an application like so:

```cpp
TimerEventsClass timerEvents;

TimerEvent blinkEvent(  "Blink Event",      // Name of the event
                            1000,           // Period, ms. Valid [0, MAX_VALUE_UINT32]
                            ledCallback,    // On execution callback function 
                            true,           // Enabled
                            -1,             // Duration, ms. Valid [0, MAX_VALUE_LONG]. Default: -1
                            nullptr);       // On finish callback function. Default: nullptr

timerEvents.add(&blinkEvent); // Add blinkEvent to task manager
.
.
.
void loop() {
    timerEvents.tasks(); // Loop over timer tasks and execute as appropriate
}
```

## Improving the Original Code

Let's work through the original code posted at the top of this file and improve it using the updated code post above.

### Event 1: Heartbeat

We will start with `Event 1` where we want to send a heartbeat signal every 2000 ms.
We will define the event function as:

```cpp
void sendHeartbeatSignal() {
    LoRa.beginPacket();
    LoRa.print("HB");
    LoRa.endPacket();
    Serial.println("Sending HB");
}
```

Then, we can define the task as:

```cpp
TimerEvent heartbeatEvent(  "Blink Event",          // Name of the event
                            2000,                   // Period, ms. Valid [0, MAX_VALUE_UINT32]
                            sendHeartbeatSignal,    // On execution callback function 
                            true,                   // Enabled
                            -1,                     // Duration, ms. Valid [0, MAX_VALUE_LONG]. Default: -1
                            nullptr);               // On finish callback function. Default: nullptr
```

### Event 2: Sending Joystick X-Values

We can do the same thing to the second event, but this time, there will be a conditional we will check for if the event is enabled before checking to execute it.
Let's first define the callback function:

```cpp
void sendJoystickXValue() {
    LoRa.beginPacket();
    LoRa.print(xvalue);
    LoRa.endPacket();
    Serial.print("X: ");
    Serial.println(xvalue);
}
```

Next, we can define the task and the conditional implementation:

```cpp
TimerEvent sendJoystickXEvent(  "Joystick Horizontal Send Event",   // Name of the event
                                200,                                // Period, ms. Valid [0, MAX_VALUE_UINT32]
                                sendJoystickXValue,                 // On execution callback function 
                                true,                               // Enabled
                                -1,                                 // Duration, ms. Valid [0, MAX_VALUE_LONG]. Default: -1
                                nullptr);                           // On finish callback function. Default: nullptr
.
.
.
void loop() {
    // Read in joystick x value as xvalue
    xvalue != lastXValue ? sendJoystickXEvent.enable() : sendJoystickXEvent.disable() // Enable send event if xvalue has changed, otherwise don't send

    // Execute tasks if they are enabled and the appropriate time period has passed
    timerEvents.tasks();
}
```

### Event 3: Sending Joystick Y-Values

Let's first define the callback function for the third event:

```cpp
void sendJoystickYValue() {
    LoRa.beginPacket();
    LoRa.print(yvalue);
    LoRa.endPacket();
    Serial.print("Y: ");
    Serial.println(yvalue);
}
```

Next, we can define the task and the conditional implementation:

```cpp
TimerEvent sendJoystickXEvent(  "Joystick Vertical Send Event",     // Name of the event
                                300,                                // Period, ms. Valid [0, MAX_VALUE_UINT32]
                                sendJoystickYValue,                 // On execution callback function 
                                true,                               // Enabled
                                -1,                                 // Duration, ms. Valid [0, MAX_VALUE_LONG]. Default: -1
                                nullptr);                           // On finish callback function. Default: nullptr
.
.
.
void loop() {
    // Read in joystick y value as yvalue
    yvalue != lastYValue ? sendJoystickYEvent.enable() : sendJoystickYEvent.disable() // Enable send event if yvalue has changed, otherwise don't send

    // Execute tasks if they are enabled and the appropriate time period has passed
    timerEvents.tasks();
}
```