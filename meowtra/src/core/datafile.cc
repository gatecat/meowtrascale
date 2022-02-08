#include "datafile.h"

MEOW_NAMESPACE_BEGIN

std::pair<std::string_view, std::string_view> split_view(std::string_view view, char delim) {
	auto pos = view.find(delim);
	MEOW_ASSERT(pos != std::string_view::npos);
	return std::make_pair(
		view.substr(0, pos),
		view.substr(pos+1)
	);
}
int32_t parse_i32(std::string_view view) {
	char *end = nullptr;
	auto result = std::strtol(view.data(), &end, 0);
	MEOW_ASSERT(end != view.data());
	return result;
}
uint32_t parse_u32(std::string_view view) {
	char *end = nullptr;
	auto result = std::strtoul(view.data(), &end, 0);
	MEOW_ASSERT(end != view.data());
	return result;
}

MEOW_NAMESPACE_END
