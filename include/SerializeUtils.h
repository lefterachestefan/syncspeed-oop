#ifndef SERIALIZEUTILS_H
#define SERIALIZEUTILS_H

#include <iostream>
#include <string>

namespace SerializeUtils {

inline void write_string(std::ostream& os, const std::string& str) {
	size_t len = str.size();
	os.write(reinterpret_cast<const char*>(&len), sizeof(len));
	os.write(str.data(), len);
}

inline std::string read_string(std::istream& is) {
	size_t len = 0;
	is.read(reinterpret_cast<char*>(&len), sizeof(len));
	if (!is) {
		return "";
	}
	std::string str(len, '\0');
	is.read(str.data(), len);
	return str;
}

}  // namespace SerializeUtils
#endif
