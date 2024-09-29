use std::collections::VecDeque;

pub type Measurement = egui::plot::PlotPoint;

#[derive(Debug)]
pub struct MeasurementWindow {
    pub values: VecDeque<Measurement>,
    pub look_behind: usize,
}

impl MeasurementWindow {
    pub fn new_with_look_behind(look_behind: usize) -> Self {
        Self {
            values: VecDeque::new(),
            look_behind,
        }
    }

    pub fn add(&mut self, measurement: Measurement) {
        if let Some(last) = self.values.back() {
            if measurement.x < last.x {
                self.values.clear()
            }
        }

        self.values.push_back(measurement);

        let limit = self.values.back().unwrap().x - (self.look_behind as f64);
        while let Some(front) = self.values.front() {
            if front.x >= limit {
                break;
            }
            self.values.pop_front();
        }
    }

    pub fn plot_values(&self) -> egui::plot::PlotPoints {
        egui::plot::PlotPoints::Owned(Vec::from_iter(self.values.iter().copied()))
    }
}
