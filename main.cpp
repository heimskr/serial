#include <iostream>
#include <stdint.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

enum class Parity: char {None = 'n', Odd = 'o', Even = 'e', Mark = 'm', Space = 's'};
enum class FlowControl: char {None = 'N', XON_XOFF = 'X', RTS_CTS = 'R', DSR_DTR = 'D'};

bool parseLong(const std::string &str, int64_t &out, int base = 10) {
	char *endptr;
	out = strtoll(str.c_str(), &endptr, base);
	return static_cast<size_t>(endptr - str.c_str()) == str.size();
}

bool parseLong(const char *str, int64_t &out, int base = 10) {
	char *endptr;
	out = strtoll(str, &endptr, base);
	return static_cast<size_t>(endptr - str) == strlen(str);
}

std::vector<std::string> split(const std::string &str, const std::string &delimiter, bool condense = false) {
	if (str.empty())
		return {};

	size_t next = str.find(delimiter);
	if (next == std::string::npos)
		return {str};

	std::vector<std::string> out {};
	const size_t delimiter_length = delimiter.size();
	size_t last = 0;

	out.push_back(str.substr(0, next));

	while (next != std::string::npos) {
		last = next;
		next = str.find(delimiter, last + delimiter_length);
		std::string sub = str.substr(last + delimiter_length, next - last - delimiter_length);
		if (!sub.empty() || !condense)
			out.push_back(std::move(sub));
	}

	return out;
}

bool matches(char ch, const char *chars) {
	return std::string(chars).find(ch) != std::string::npos;
}

int main(int argc, char **argv) {
	int64_t baud = 115200;
	Parity parity = Parity::None;
	int semistopbits = 2; // 2, 3 or 4
	FlowControl flow = FlowControl::None;
	int64_t databits = 8;

	if (argc != 2 && argc != 3) {
		std::cerr << "Usage: serial <device> [serial configuration]\n";
		return 1;
	}

	const char *path = argv[1];
	
	if (argc == 3) {
		for (const std::string &arg: split(argv[2], ",", true)) {
			if (arg.size() == 1) {
				const char ch = arg[0];
				if (matches(ch, "noems"))
					parity = static_cast<Parity>(ch);
				else if (matches(ch, "NXRD"))
					flow = static_cast<FlowControl>(ch);
				else if (matches(ch, "56789"))
					databits = ch - '0';
				else if (matches(ch, "12"))
					semistopbits = 2 * (ch - '0');
				else {
					std::cerr << "Invalid argument: " << arg << "\n";
					return 1;
				}
			} else if (arg == "1.5") {
				semistopbits = 3;
			} else {
				int64_t newbaud;
				if (!parseLong(arg, newbaud)) {
					std::cerr << "Invalid argument: " << arg << "\n";
					return 1;
				} else
					baud = newbaud;
			}
		}
	}
}
