pub struct PID {
    kp: f64,
    ki: f64,
    kd: f64,
    step_time: f64,
    target_speed: f64,
    last_error: f64,
    integral: f64,
}

impl PID {
    pub fn new(kp: f64, ki: f64, kd: f64, step_time: f64, target_speed: f64) -> Self {
        Self {
            kp,
            ki,
            kd,
            step_time,
            target_speed,
            last_error: 0.0,
            integral: 0.0,
        }
    }

    pub fn update(&mut self, current_speed: f64, current_throttle: f64) -> f64 {
        let speed_error = self.target_speed - current_speed;
        let proportional = self.kp * speed_error;

        self.integral += speed_error * self.step_time;

        let integral = self.ki * self.integral;
        let derivative = self.kd * (speed_error - self.last_error) / self.step_time;

        self.last_error = speed_error;

        let throttle_adjustment = proportional + integral + derivative;

        (current_throttle + throttle_adjustment).clamp(0.0, 1.0)
    }
}
