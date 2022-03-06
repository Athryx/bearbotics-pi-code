#pragma once

#include <string>
#include <optional>

enum class ErrorType: int {
	Ok,
	// an internal error occured
	Internal,
	// an unknown erorr occured
	Unknown,
	// the requested operation was not valid
	InvalidOperation,
	// failed to open a certain resource
	ResourceUnavailable,
	// TODO: maybe these errors are to specific and should not have a dedicated type
	RemoteViewingStateChangeError,
	RemoteViewingEosError,
	RemoteViewingPipelineError,
};

#define ERROR_CONSTRUCTOR_DEFS(ctor_name, error_type)										\
static inline Error ctor_name() { return Error(error_type); }										\
static inline Error ctor_name(const std::string& message) { return Error(error_type, message); }	\
static inline Error ctor_name(std::string&& message) { return Error(error_type, message); }

// TODO: add domain codes
class [[nodiscard]] Error {
	public:
		Error(ErrorType type);
		Error(ErrorType type, const std::string& message);
		Error(ErrorType type, std::string&& message);
		~Error();

		static inline Error ok() { return Error(ErrorType::Ok); }

		ERROR_CONSTRUCTOR_DEFS(internal, ErrorType::Internal)
		ERROR_CONSTRUCTOR_DEFS(unknown, ErrorType::Unknown)
		ERROR_CONSTRUCTOR_DEFS(invalid_operation, ErrorType::InvalidOperation)
		ERROR_CONSTRUCTOR_DEFS(resource_unavailable, ErrorType::ResourceUnavailable)

		// parse from string returned by to_string
		static std::optional<Error> from_string(const std::string& string);
		static std::optional<Error> from_string(std::string&& string);

		// returns string to be parsed later
		std::string to_string() const;
		// returns string to be displayed to user
		std::string display_string() const;

		ErrorType type() const;
		const std::string& message() const;

		bool is_ok() const;
		bool is_err() const;

	private:
		ErrorType m_type;
		std::string m_message;
};
