use std::process::{Command, Child};
use crate::Args;

pub struct Vision {
	command: Command,
	process: Option<Child>,
}

impl Vision {
	pub fn new(path: &str, mqtt_host: &str, mqtt_port: u16, vision_args: &[&str]) -> Self {
		let mut command = Command::new(path);

		command.args(["-m", mqtt_host])
			.args(["-p", &mqtt_port.to_string()])
			.args(vision_args);

		Vision {
			command,
			process: None,
		}
	}

	pub fn start(&mut self) -> bool {
		if self.process.is_none() {
			let child = self.command.spawn();
			if let Ok(child) = child {
				self.process = Some(child);
				true
			} else {
				false
			}
		} else {
			false
		}
	}

	pub fn stop(&mut self) -> bool {
		if let Some(mut child) = self.process {
			child.kill().is_ok()
		} else {
			false
		}
	}
}
