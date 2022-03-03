use std::process::{Command, Child};

use anyhow::{Result, Error, Context};

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

	pub fn start(&mut self) -> Result<()> {
		if self.process.is_some() {
			return Err(Error::msg("vision is not stopped"))
		}
		self.process = Some(self.command.spawn().context("could not spawn vision")?);
		Ok(())
	}

	pub fn stop(&mut self) -> Result<()> {
		let process = self.process.as_mut().ok_or_else(|| Error::msg("vision is not started"))?;
		process.kill().context("could not stop vision, or vision was already stopped due to an erouneous exit")
	}
}
