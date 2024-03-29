//
// Created by gabriele on 23/12/21.
//

#include "utils.h"

namespace fs = std::filesystem;

bool is_file(char *url) {
	auto str = std::string{url};
	return str.find(".mp4") != std::string::npos;
}

bool is_file_str(const std::string &str) {
	return str.find(".mp4") != std::string::npos;
}

void move_file(const std::string &source, const std::string &dest) {
	try {
		auto dest_path = fs::path(dest);
		auto dest_folder = is_file_str(dest) ? dest_path.parent_path() : dest_path;
		auto dest_file = is_file_str(dest) ? dest_path : dest_path / fs::path(source).filename();

		// create parent folder tree
		if (!exists(dest_folder)) {
			fs::create_directories(dest_folder);
		}

		// overwrite destination file
		if (exists(dest_file)) {
			fs::remove(dest_file);
		}

		fs::copy_file(source, dest_file);
		fs::remove(source);

	} catch (std::runtime_error &_e) {
		throw fsException("Unable to move " + source + " to " + dest);
	}
}

void delete_file(const std::string &filename) {
	try {
		auto dest_path = fs::path(filename);

		// delete file
		if (exists(dest_path)) {
			fs::remove(dest_path);
		}

	} catch (std::runtime_error &_e) {
		throw fsException("Unable to delete " + filename);
	}
}

/**
 * Retrive a string of the path where store output media file
 *
 * @param path filesystem path
 * @return a string with output path
 */
std::string get_default_path(const fs::path &path) {
	return (path / "output.mp4").string();
}

void log_info(const std::string &str) {
	if (LOGGING >= 1) {
		std::cout << "[INFO] " + str << std::endl;
	}
}

void log_debug(const std::string &str) {
	if (LOGGING >= 2) {
		std::cout << "[DEBUG] " + str << std::endl;
	}
}

void log_error(const std::string &str) {
	std::cout << "[ERROR] " + str << std::endl;
}

void print_version() {
	std::cout << PROJECT_NAME << " version: " << PROJECT_VER << std::endl;
}

void print_helper() {
	print_version();

	std::cout << "\nUSAGE: recs <args>" << std::endl;
	std::cout << "  -v              Set logging to INFO level" << std::endl;
	std::cout << "  -w              Set loggign to DEBUG level" << std::endl;
	std::cout << "  -V, --version   Print current software version" << std::endl;
	std::cout << "  -h, --help      Show this helper" << std::endl;
}

/**
 * Retrive local current datetime
 *
 * @return a string `%Y-%m-%d_%H.%M.%S` for Windows and `%Y-%m-%d_%H:%M:%S` for unix systems
 */
std::string get_current_time_str() {
	char buff[100];
	auto time_ref = std::time(nullptr);
#ifdef WIN32
	std::strftime(buff, sizeof(buff), "%Y-%m-%d_%H.%M.%S", std::localtime(&time_ref));
#else
	std::strftime(buff, sizeof(buff), "%Y-%m-%d_%H:%M:%S", std::localtime(&time_ref));
#endif

	return buff;
}
