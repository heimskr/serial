// Credit: https://stackoverflow.com/a/6947758

#include <fcntl.h>
#include <iostream>
#include <stdint.h>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#ifndef CMSPAR
#define CMSPAR 0x40000000
#endif

enum class Parity: char {None = 'n', Odd = 'o', Even = 'e', Mark = 'm', Space = 's'};
enum class FlowControl: char {None = 'N', XON_XOFF = 'X', RTS_CTS = 'R'/* , DSR_DTR = 'D' */};

bool parseUlong(const std::string &str, uint64_t &out, int base = 10) {
	char *endptr;
	out = strtoull(str.c_str(), &endptr, base);
	return static_cast<size_t>(endptr - str.c_str()) == str.size();
}

bool parseUlong(const char *str, uint64_t &out, int base = 10) {
	char *endptr;
	out = strtoull(str, &endptr, base);
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
	uint64_t baud = 115200;
	Parity parity = Parity::None;
	int semistopbits = 2; // 2, 3 or 4
	FlowControl flow = FlowControl::None;
	int databits = 8;

	if (argc != 2 && argc != 3) {
		std::cerr << "Usage: serial <device> [serial configuration]\n";
		return 1;
	}

	const char *path = argv[1];

	if (argc == 3)
		for (const std::string &arg: split(argv[2], ",", true)) {
			if (arg.size() == 1) {
				const char ch = arg[0];
				if (matches(ch, "noems"))
					parity = static_cast<Parity>(ch);
				else if (matches(ch, "NXR"/* "D" */))
					flow = static_cast<FlowControl>(ch);
				else if (matches(ch, "5678"))
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
				uint64_t newbaud;
				if (!parseUlong(arg, newbaud)) {
					std::cerr << "Invalid argument: " << arg << "\n";
					return 1;
				} else
					baud = newbaud;
			}
		}


	if (semistopbits == 3 && databits != 5) {
		std::cerr << "Stop bits set to 1.5, but data bits set to " << databits << " instead of 5\n";
		return 1;
	}

	const int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_SYNC);
	if (fd < 0) {
		std::cerr << "Couldn't open " << path << ": " << strerror(errno) << "\n";
		return 1;
	}

	if (fcntl(fd, F_SETFL, 0) == -1) {
		std::cerr << "fcntl failed: " << strerror(errno) << "\n";
		return 1;
	}

	termios tty;
	if (tcgetattr(fd, &tty)) {
		std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
		return 1;
	}

	cfsetspeed(&tty, baud);
	tty.c_cflag = tty.c_cflag & ~CSIZE;
	switch (databits) {
		case 5: tty.c_cflag |= CS5; break;
		case 6: tty.c_cflag |= CS6; break;
		case 7: tty.c_cflag |= CS7; break;
		case 8: tty.c_cflag |= CS8; break;
		default:
			std::cerr << "Invalid data bits: " << databits << "\n";
			return 1;
	}

	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 5;

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	if (flow == FlowControl::XON_XOFF)
		tty.c_iflag |= IXON | IXOFF;

	tty.c_cflag |= CLOCAL | CREAD;


	tty.c_cflag &= ~(PARENB | PARODD | CMSPAR);
	switch (parity) {
		case Parity::Even:  tty.c_cflag |= PARENB; break;
		case Parity::Odd:   tty.c_cflag |= PARENB | PARODD; break;
		case Parity::Mark:  tty.c_cflag |= CMSPAR | PARENB | PARODD; break;
		case Parity::Space: tty.c_cflag |= CMSPAR | PARENB; break;
		case Parity::None:  break;
		default:
			std::cerr << "Invalid parity: " << static_cast<int>(parity) << "\n";
			return 1;
	}

	if (semistopbits == 2)
		tty.c_cflag &= ~CSTOPB;
	else
		tty.c_cflag |= CSTOPB;

	if (flow == FlowControl::RTS_CTS)
		tty.c_cflag |= CRTSCTS;
	else
		tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &tty)) {
		std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
		return 1;
	}

	char ch;
	for (;;) {

		// int sstatus = select(1, &fds, nullptr, nullptr, nullptr);
		// std::cout << "(" << sstatus << ")\n";
		// std::cout << "Reading...\n";
		ssize_t status = read(fd, &ch, 1);
		// std::cout << "Read.\n";
		if (status < 0) {
			std::cerr << "Reading failed: " << strerror(errno) << "\n";
			return 1;
		} else if (0 < status) {
			std::cout << ch;
		}
	}

	return 0;
}
