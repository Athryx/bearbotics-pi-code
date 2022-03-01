use gstreamer::prelude::{ElementExt, GstBinExtManual, Continue};
use gstreamer::{State, MessageType, ElementFactory, Pipeline, Element, Bus, Message};

fn tmp() {
	let source = ElementFactory::make("videotestsrc", Some("source")).unwrap();
	let sink = ElementFactory::make("autovideosink", Some("sink")).unwrap();

	let pipeline = Pipeline::new(Some("test-pipeline"));

	pipeline.add_many(&[&source, &sink]).unwrap();

	source.link(&sink).unwrap();
	//source.set_properties(&[("pattern", &)]);

	pipeline.set_state(State::Playing).unwrap();

	let bus = pipeline.bus().unwrap();
	let message = bus.timed_pop_filtered(None, &[MessageType::Error, MessageType::Eos]).unwrap();

	pipeline.set_state(State::Null).unwrap();

	if message.type_() == MessageType::Error {
		panic!("message is an error");
	}
}

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
	pub fn start(&mut self) -> bool {
		self.pipeline.set_state(State::Playing).is_ok()
	}

	// returns true if sucessfully stopped
	pub fn stop(&mut self) -> bool {
		self.pipeline.set_state(State::Paused).is_ok()
	}
}

impl Drop for RemoteViewing {
	fn drop(&mut self) {
		if self.pipeline.set_state(State::Null).is_err() {
			warn!("Could not set pipeline state to null");
		}
	}
}
