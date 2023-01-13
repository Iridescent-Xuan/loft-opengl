#pragma once

#include <string>

bool printScreen(const std::string& reference_file, const std::string& file_path, 
	int height, int width, std::string* err);