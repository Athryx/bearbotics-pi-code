use std::process::{Command, Child};

pub struct Vision {
	command: Command,
	process: Option<Child>,
}

impl Vision {
	pub fn new(path: &str) -> Self {
		Vision {
			command: Command::new(path),
			process: None,
		}
	}
}
