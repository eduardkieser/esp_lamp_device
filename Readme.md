Dimmable Smart Lamp


## Desired Behavior


Id like the lamp to be controllable via both the web interface and and the potentiometer.
To do this the lamp will be on one of two modes:

Remoter mode: The lamp enters remote mode when it receives a command from the web interface. It will also need to store the last potentiometer value so it can tell if the potentiometer is being used. If the filtered value changes by more than 5% of it's range, it will switch to potentiometer mode.

Potentiometer mode: The lamp enters potentiometer mode when the filtered value changes by more than 5% of it's range. It will then set the pwm value to the potentiometer value and switch to potentiometer mode.

