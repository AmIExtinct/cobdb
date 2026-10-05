#include "application.hpp"
#include "database.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

void printUsage(const char* program) {
	std::fprintf(stderr, "Usage:\n  %s new <database-file>\n  %s open <database-file>\n", program, program);
	std::fprintf(stderr, "Alias: %s create <database-file>\n", program);
}

} // namespace

int main(int argc, char** argv) {
	if (argc != 3) {
		printUsage(argv[0]);
		return 2;
	}

	const std::string command(argv[1]);
	const std::string filePath(argv[2]);
	if (command == "new" || command == "create") {
		std::ifstream existing(filePath.c_str(), std::ios::binary);
		if (existing.good()) {
			std::fprintf(stderr, "Refusing to overwrite existing file: %s\n", filePath.c_str());
			return 1;
		}
		cob::Database database(filePath);
		std::string error;
		if (!database.save(error)) {
			std::fprintf(stderr, "Could not create database: %s\n", error.c_str());
			return 1;
		}
		std::printf("Created database: %s\n", filePath.c_str());
		return 0;
	}
	if (command == "open") {
		cob::Database database;
		std::string error;
		if (!cob::Database::load(filePath, database, error)) {
			std::fprintf(stderr, "Could not open database: %s\n", error.c_str());
			return 1;
		}
		cob::Application application(database);
		return application.run();
	}

	printUsage(argv[0]);

#ifdef __APPLE__
    // later .. 
#endif
	return 2;
}
