#ifndef SERIALIZEUTILS_H
#define SERIALIZEUTILS_H

#include <iostream>
#include <string>

namespace SerializeUtils {

inline void write_string(std::ostream& os, const std::string& str) {
	size_t len = str.size();
	os.write(reinterpret_cast<const char*>(&len), sizeof(len));
	// TODO: check later for all CPU's if this conversion to long is okay
	os.write(str.data(), (long)len);
}

inline std::string read_string(std::istream& is) {
	size_t len = 0;
	is.read(reinterpret_cast<char*>(&len), sizeof(len));
	if (!is) {
		return "";
	}
	std::string str(len, '\0');
	// TODO: check later for all CPU's if this conversion to long is okay
	is.read(str.data(), (long)len);
	return str;
}

}  // namespace SerializeUtils
#endif
