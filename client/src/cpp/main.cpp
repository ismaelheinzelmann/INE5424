
#include <iostream>
#include <ostream>

#include "../../../lib/src/header/ConfigParser.h"
#include "../../../lib/src/header/CRC32.h"

int main() {
  auto configs = ConfigParser::parseNodes("../../../config/config");
  return 0;
}