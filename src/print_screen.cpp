#include "print_screen.h"

#include <memory>
#include <sstream>
#include <stdlib.h>

#include <glad/glad.h>

#define BMP_HEADER_LENGTH 54
#define BYTES_PER_PIXEL 3

bool printScreen(const std::string& reference_file, const std::string& file_path, 
	int height, int width, std::string* err) {

	// compute the aligned length of bmp file
	int i = width * BYTES_PER_PIXEL;
	while (i % 4 != 0) ++i;
	int pixel_data_length = i * height;

	// allocate memory
	GLubyte* pixel_data = new GLubyte[pixel_data_length];
	
	std::stringstream errss;

	// open the reference and target bmp file
	FILE* dummy = fopen(reference_file.c_str(), "rb");
	if (!dummy) {
		errss << "Cannot open file [" << reference_file << "]" << std::endl;
		if (err) {
			(*err) = errss.str();
		}
		return false;
	}
	FILE* output = fopen(file_path.c_str(), "wb");
	if (!output) {
		errss << "Cannot open file [" << file_path << "]" << std::endl;
		if (err) {
			(*err) = errss.str();
		}
		return false;
	}

	// read pixels from color buffer
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixel_data);

	// copy the bmp header
	GLubyte BMP_header[BMP_HEADER_LENGTH];
	fread(BMP_header, sizeof(BMP_header), 1, dummy);
	fwrite(BMP_header, sizeof(BMP_header), 1, output);

	// change the resolution
	fseek(output, 0x0012, SEEK_SET);
	fwrite(&width, sizeof(width), 1, output);
	fwrite(&height, sizeof(height), 1, output);

	// write pixel
	fseek(output, 0, SEEK_END);
	fwrite(pixel_data, pixel_data_length, 1, output);

	fclose(dummy);
	fclose(output);
	delete[]pixel_data;

	return true;
}