use gstreamer::prelude::{ElementExt, GstBinExtManual};
use gstreamer::{State, MessageType, ElementFactory, Pipeline};

fn main() {
	env_logger::init();

	gstreamer::init().unwrap();

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
