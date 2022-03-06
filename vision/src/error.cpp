#include "error.h"

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

// parse from string returned by to_string
std::optional<Error> Error::from_string(const std::string& string) {
}

std::optional<Error> Error::from_string(std::string&& string) {
}

// returns string to be parsed later
std::string Error::to_string() const {
}

// returns string to be displayed to user
std::string Error::display_string() const {
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
