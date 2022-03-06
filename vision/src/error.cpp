#include "error.h"
#include "logging.h"

Error::Error(ErrorType type):
m_type(type),
m_message() {}

Error::Error(ErrorType type, const std::string& message):
m_type(type),
m_message(message) {}

Error::Error(ErrorType type, std::string&& message):
m_type(type),
m_message(message) {}

Error::~Error() {}

std::string Error::serialize() const {
}

std::optional<Error> Error::deserialize(const std::string& string) {
}

std::string Error::to_string() const {
}

ErrorType Error::type() const {
	return m_type;
}

const std::string& Error::message() const {
	return m_message;
}

bool Error::is_ok() const {
	return m_type == ErrorType::Ok;
}

bool Error::is_err() const {
	return m_type != ErrorType::Ok;
}

void Error::ignore() const {
	if (m_type != ErrorType::Ok) {
		lg::warn("ignoring error");
	}
}
