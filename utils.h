#pragma once

#include <string>
#include <set>

int char_to_int(char c);

size_t char_to_size_t(char c);

std::string ToLower(std::string input);

std::string GetUserInput(std::string question, std::string error_message,
                         std::set<std::string> acceptable_answers);

std::string AppDataPath();