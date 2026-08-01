# Otto Throttle

Tired of manually adjusting your throttle in cruise when all you want to do is make a cup of coffee? Afraid something might go wrong while you´re gone? Don´t worry! Otto is here to take care of you.

Otto Throttle is a configurable PID controller based auto-throttle plugin for X-Plane 12, written with love in C++.

## Requirements

-   X-Plane 12
-   Windows (x86-64), Linux (x86-64) or MacOS (arm64)

*Disclaimer: The plugin has only been tested on Windows.*

## Installation

1.  Download the latest version from the releases page.
2.  Place the extracted `Otto Throttle` folder inside your plugins folder:
    ```
    C:\<PATH TO XP>\Resources\plugins\Otto Throttle
    ```

## Configuration

Otto will look for a `otto-throttle.cfg` file in the directory of the currently loaded aircraft. This allows us to have one configuration per aircraft. Which is essential to get the propper auto-throttle behaviour for the aircraft we are flying. The configuration file contains the PID parameters and the update frequency in hertz. You can increase the update frequency to get smoother throttle motion, but this may effect performance, 60hz is a good number.

Here is an example configuration file for the FlyJSim 732 TwinJet V4:
```toml
kp = 0.00001
ki = 0.0
kd = 0.00035
hz = 60
```

Lines starting with `#` or `;` are treated as comments and ignored. 

## Usage

Otto can be toggled on/off via the plugin menu. There is also a button called "Reinflate Otto", which reloads the config file. This is useful when trying to tune the PID parameters. Otto does not block throttle axis input, so a noisy axis will effect otto's efforts at moving the throttle. I suggest keeping the throttle at max or min to avoid noisy inputs.
