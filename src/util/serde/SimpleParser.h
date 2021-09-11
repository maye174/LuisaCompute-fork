#pragma once
#include <util/serde/Common.h>
namespace toolhub::db {
struct ParsingException {
	std::string message;
	ParsingException() {}
	ParsingException(std::string&& msg)
		: message(std::move(msg)) {}
};
}// namespace toolhub::db