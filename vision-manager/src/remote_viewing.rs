use gstreamer::prelude::{ElementExt, GstBinExtManual, Continue};
use gstreamer::{State, MessageType, ElementFactory, Pipeline, Element, Bus, Message};
use anyhow::Result;

pub struct RemoteViewing {
	pipeline: Element,
	bus: Bus,
}

impl RemoteViewing {
	// FIXME: doesn't work
	fn pipeline_watch(bus: &Bus, message: &Message) -> Continue {
		debug!("pipeline_watch called with message: {:?}", message);
		match message.type_() {
			MessageType::Error => {
				todo!();
			},
			MessageType::Eos => {
				error!("unexpected end of stream message from pipeline");
				panic!("unexpected end of stream message from pipeline");
			},
			_ => Continue(true),
		}
	}

	pub fn new(host: &str, port: u16) -> Option<RemoteViewing> {
		info!("creating new gstreamer pipeline");

		let pipeline = gstreamer::parse_launch(
			&format!("v4l2src device=\"/dev/video0\" ! video/x-raw,width=320,height=240 ! videoscale ! videoconvert ! x264enc tune=zerolatency bitrate=500 speed-preset=superfast \
				! rtph264pay config-interval=10 pt=96 ! udpsink host={} port={}", host, port)
		).ok()?;

		let bus = pipeline.bus()?;
		bus.add_watch(Self::pipeline_watch).expect("could not set watch function on pipeline");

		Some(RemoteViewing {
			pipeline,
			bus,
		})
	}

	// returns true if sucessfully started
	pub fn start(&mut self) -> Result<()> {
		anyhow::Context::context(self.pipeline.set_state(State::Playing), "could not start remote viewing")?;
		Ok(())
	}

	// returns true if sucessfully stopped
	pub fn stop(&mut self) -> Result<()> {
		anyhow::Context::context(self.pipeline.set_state(State::Paused), "could not stop remote viewing")?;
		Ok(())
	}
}

impl Drop for RemoteViewing {
	fn drop(&mut self) {
		if self.pipeline.set_state(State::Null).is_err() {
			warn!("Could not set pipeline state to null");
		}
	}
}
