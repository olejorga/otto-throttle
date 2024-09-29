pub struct PID {
    pub kp: f64,
    pub ki: f64,
    pub kd: f64,
    pub target: f64,
    prev_error: f64,
    integral: f64,
}

impl PID {
    pub fn new(kp: f64, ki: f64, kd: f64, target: f64) -> Self {
        Self {
            kp,
            ki,
            kd,
            target,
            prev_error: 0.0,
            integral: 0.0,
        }
    }

    pub fn update(&mut self, value: f64, dt: f64) -> f64 {
        let error = self.target - value;
        let proportional = self.kp * error;

        self.integral += error * dt;

        let integral = self.ki * self.integral;
        let derivative = self.kd * (error - self.prev_error) / dt;

        self.prev_error = error;

        proportional + integral + derivative
    }
}
