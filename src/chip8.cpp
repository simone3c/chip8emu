#include "chip8.hpp"

void Chip8::load(const std::vector<uint8_t>& prog){
    assert(prog.size() < MAX_PROG_SIZE);
    std::memcpy(&ram[PC_RESET_VALUE], prog.data(), prog.size());
}
