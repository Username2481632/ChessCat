#include "utils.h"

int char_to_int(char c) { 
  return (int)c - int('0'); 
}

size_t char_to_size_t(char c) { 
  return ((size_t)c - (size_t)'0'); 
}

std::string ToLower(std::string input) {
  std::string output;
  for (size_t i = 0; i < input.length(); i++) {
    output += input[i];
  }
  return output;
}