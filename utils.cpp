#include "utils.h"
#include <cctype>
#include <iostream>

int char_to_int(char c) { 
  return (int)c - int('0'); 
}

size_t char_to_size_t(char c) { 
  return ((size_t)c - (size_t)'0'); 
}

std::string ToLower(std::string input) {
  std::string output;
  for (size_t i = 0; i < input.length(); i++) {
    output += (char)tolower(input[i]);
  }
  return output;
}

std::string GetUserInput(std::string question, std::string error_message,
                         std::set<std::string> acceptable_answers) {
  std::string response;
  std::cout << question;
  std::getline(std::cin, response);
  response = ToLower(response);
  while (!acceptable_answers.contains(response)) {
    std::cout << error_message;
    std::getline(std::cin, response);
    response = ToLower(response);
  }
  return response;
}