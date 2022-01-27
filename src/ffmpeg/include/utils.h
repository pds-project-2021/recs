//
// Created by gabriele on 23/12/21.
//

#pragma once

#include <filesystem>
#include <string>

#include "exceptions.h"

using namespace std;
namespace fs = filesystem;

/**
 * Useful wrapper for screen parameters
 */
class Screen{
	int width = 0.0;
	int height = 0.0;
	int offset_x = 0.0;
	int offset_y = 0.0;

	bool show_region = true;
  public:

	Screen() = default;
	Screen(int width, int height, int offset_x, int offset_y);
	~Screen() = default;

	void set_dimension(const string &dim);
	void set_dimension(int w, int h);
	void set_offset(const string &offset);
	void set_offset(int x, int y);
	void set_show_region(bool val);

	[[nodiscard]] string get_offset_x() const;
	[[nodiscard]] string get_offset_y() const;
	[[nodiscard]] string get_offset_str() const;
    [[nodiscard]] string get_width() const;
    [[nodiscard]] string get_height() const;
	[[nodiscard]] string get_video_size() const;
	[[nodiscard]] string get_show_region() const;
};

bool is_file(char *url);
bool is_file_str(const string &str);
void move_file(const string& source, const string &dest);