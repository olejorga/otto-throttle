use std::{
    sync::{Arc, Mutex},
    thread,
};

use measurement::{Measurement, MeasurementWindow};

mod measurement;

pub struct MonitorApp {
    include_y: Vec<f64>,
    measurements: Arc<Mutex<MeasurementWindow>>,
}

impl MonitorApp {
    fn new(look_behind: usize) -> Self {
        Self {
            measurements: Arc::new(Mutex::new(MeasurementWindow::new_with_look_behind(
                look_behind,
            ))),
            include_y: Vec::new(),
        }
    }
}

impl eframe::App for MonitorApp {
    // /// Called by the frame work to save state before shutdown.
    // /// Note that you must enable the `persistence` feature for this to work.
    // #[cfg(feature = "persistence")]
    // fn save(&mut self, storage: &mut dyn eframe::Storage) {
    //     eframe::set_value(storage, eframe::APP_KEY, self);
    // }

    /// Called each time the UI needs repainting, which may be many times per second.
    /// Put your widgets into a `SidePanel`, `TopPanel`, `CentralPanel`, `Window` or `Area`.
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            let mut plot = egui::plot::Plot::new("measurements");

            for y in self.include_y.iter() {
                plot = plot.include_y(*y);
            }

            plot.show(ui, |plot_ui| {
                plot_ui.line(egui::plot::Line::new(
                    self.measurements.lock().unwrap().plot_values(),
                ));
            });
        });

        ctx.request_repaint();
    }
}

fn main() {
    let mut app = MonitorApp::new(1000);

    app.include_y = vec![];

    let native_options = eframe::NativeOptions::default();

    let monitor_ref = app.measurements.clone();

    thread::spawn(move || {
        monitor_ref.lock().unwrap().add(Measurement::new(0, 0));
    });

    eframe::run_native("Monitor app", native_options, Box::new(|_| Box::new(app)));
}
