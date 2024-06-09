-------------------
-- OTTO THROTTLE --
-------------------
--- by olejorga ---
-------------------
------- 1.1 -------
-------------------


-- Create a toggle button in the FlyWithLua macro context menu
add_macro("Engage O/T", "otto_throttle_on = true", "otto_throttle_on = false")


-- Get the aircraft's current speed, target speed & the current sim rate
-- And create a writable var for the throttle setting
dataref("speed", "sim/flightmodel/position/indicated_airspeed", "readonly")
dataref("target_speed", "sim/cockpit2/autopilot/airspeed_dial_kts", "readonly")
dataref("throttle_setting", "sim/cockpit2/engine/actuators/throttle_ratio_all", "writable")
dataref("sim_rate", "sim/time/sim_speed", "readonly")


-- Save the current speed as the last speed recorded
local last_speed = speed


function adjust_thrust()
  -- Adjusts the throttle setting either up or down, 
  -- based on the current speed & the target speed.

  function increase_thrust(factor)
    -- Increases the throttle setting by
    -- a factor of x on every call

    -- Makes sure throttle is not set above the highest setting
    if (throttle_setting + factor) == 1 then
      throttle_setting = 1
    elseif throttle_setting < 1 then
      throttle_setting = throttle_setting + factor
    end
  end

  function decrease_thrust(factor)
    -- Decreases the throttle setting by
    -- a factor of x on every call

    -- Makes sure throttle is not set below the lowest setting
    if (throttle_setting - factor) == 1 then
      throttle_setting = 0
    elseif throttle_setting > 0 then
      throttle_setting = throttle_setting - factor
    end
  end

  -- If "otto-throttle" is enabled & the sim is not paused, do this
  if otto_throttle_on == true and sim_rate ~= 0 then
    -- Calculate the diff between current & target speed
    local difference = math.abs((speed - target_speed))

    -- If the speed is lower than the target and last speed, increase
    if speed < target_speed and speed < last_speed then
      -- If diff is less than 5 increase gently, if not, fast
      if difference < 5 then
        increase_thrust(0.0001)
      else
        increase_thrust(0.001)
      end
    end

    -- If the speed is higher than the target and last speed, decrease
    if speed > target_speed and speed > last_speed then
      -- If diff is less than 5 decrease gently, if not, fast
      if difference < 5 then
        decrease_thrust(0.0001)
      else
        decrease_thrust(0.001)
      end
    end

    -- Updates the last recorded speed
    last_speed = speed
  end
end


-- Adjust throttles x (current FPS) times per sec
do_every_frame("adjust_thrust()")
