#include "chip8.hpp"

void Chip8::load(const std::vector<uint8_t>& prog){
    assert(prog.size() < MAX_PROG_SIZE);
    std::memcpy(&ram[PC_RESET_VALUE], prog.data(), prog.size());
}

void Chip8::handle_0_instr(const instruction_t& instr){
    switch(instr.NNN){
        // clear screen
        case 0x0E0:
            screen.reset();
        break;
        // return from subroutine
        case 0x0EE:
            PC = stack.top();
            stack.pop();
        break;
        // 0NNN
        default:
            LOGLN("Unreachable! Instruction 0x0{:03X}", instr.NNN);
            std::unreachable();
    }
};

void Chip8::handle_1_instr(const instruction_t& instr){
    // only 1NNN here, JMP
    PC = instr.NNN;
}

void Chip8::handle_2_instr(const instruction_t& instr){
    // only 2NNN, jump to subroutine
    stack.push(PC);
    PC = instr.NNN;
}

void Chip8::handle_3_instr(const instruction_t& instr){
    // only 3XNN, skip conditionally
    if(V[instr.X] == instr.NN){
        PC += 2;
    }
}

void Chip8::handle_4_instr(const instruction_t& instr){
    // only 4XNN, skip conditionally
    if(V[instr.X] != instr.NN){
        PC += 2;
    }
}

void Chip8::handle_5_instr(const instruction_t& instr){
    // only 5XY0, skip conditionally
    if(V[instr.X] == V[instr.Y]){
        PC += 2;
    }
}

void Chip8::handle_6_instr(const instruction_t& instr){
    // only 6XNN, set register
    V[instr.X] = instr.NN;
}

void Chip8::handle_7_instr(const instruction_t& instr){
    // only 7XNN, add to register
    V[instr.X] += instr.NN;
}

void Chip8::handle_8_instr(const instruction_t& instr){
    uint8_t tmp;
    switch(instr.N){
        // 8XY0, X = Y
        case 0x0:
            V[instr.X] = V[instr.Y];
        break;
        // 8XY1, X |= Y
        case 0x1:
            V[instr.X] |= V[instr.Y];
        break;
        // 8XY2, X &= Y
        case 0x2:
            V[instr.X] &= V[instr.Y];
        break;
        // 8XY3, X ^= Y
        case 0x3:
            V[instr.X] ^= V[instr.Y];
        break;
        // 8XY4, X += Y
        case 0x4:
            tmp = V[instr.X];
            V[instr.X] += V[instr.Y];

            // set VF to 1 in case of overflow
            V[0xF] = (V[instr.X] < tmp);
        break;
        // 8XY5, V[X] = V[X] - V[Y]
        case 0x5:
            tmp = V[instr.X];
            V[instr.X] -= V[instr.Y];

            // set VF to 0 in case of underflow
            V[0xF] = !(V[instr.X] > tmp);
        break;
        // 8XY6, optionally X=Y, X >>= 1, VF set to the bit
        case 0x6:
            if(copy_vy_to_vx_in_shift){
                V[instr.X] = V[instr.Y];
            }
            tmp = V[instr.X] & 0x1;
            V[instr.X] >>= 1;
            V[0xF] = tmp;
        break;
        // 8XY7, V[X] = V[Y] - V[X]
        case 0x7:
            V[instr.X] = V[instr.Y] - V[instr.X];

            // set VF to 0 in case of underflow
            V[0xF] = !(V[instr.X] > V[instr.Y]);
        break;


        // 8XYE, optionally X=Y, X <<= 1, VF set to the bit
        case 0xE:
            if(copy_vy_to_vx_in_shift){
                V[instr.X] = V[instr.Y];
            }
            tmp = V[instr.X] >> 7;
            V[instr.X] <<= 1;
            V[0xF] = tmp;
        break;

        default:
            LOGLN("Unreachable: instruction 0x8{:03X}",
                std::bit_cast<uint32_t>(instr.NNN)
            );
            std::unreachable();
        break;
    }
}

void Chip8::handle_9_instr(const instruction_t& instr){
    // only 9XY0, skip conditionally
    if(V[instr.X] != V[instr.Y]){
        PC += 2;
    }
}

void Chip8::handle_A_instr(const instruction_t& instr){
    // only ANNN, I = NNN
    I = instr.NNN;
}

void Chip8::handle_B_instr(const instruction_t& instr){
    // only BNNN, PC = NNN + V0 (default)
    // or BXNN, PC = V[X] + XNN
    uint8_t reg = 0;
    if(make_BNNN_into_BXNN){
        reg = instr.X;
    }

    PC = V[reg] + instr.NNN;
}

void Chip8::handle_C_instr(const instruction_t& instr){
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distrib(0, 255);

    // only CXNN, VX = rand() & NN
    V[instr.X] = distrib(gen) & instr.NN;
}

void Chip8::handle_D_instr(const instruction_t& instr){
    // only DXYN, draw sprite on screen
    uint8_t x = V[instr.X] % 64; // col
    uint8_t y = V[instr.Y] % 32; // row
    V[0xF] = 0;

    std::bitset<8> sprite_row;

    for(int r = 0; r < instr.N && y + r < 32; ++r){
        sprite_row = ram[I + r];

        for(int c = 0; c < 8 && x + c < 64; ++c){
            size_t screen_pix = (y + r) * 64 + x + c;
            if(sprite_row.test(7 - c)){
                if(screen.test(screen_pix)){
                    V[0xF] = 1;
                }
                screen.flip(screen_pix);
            }
        }
    }
}

void Chip8::handle_E_instr(const instruction_t& instr){
    switch(instr.NN){
        // EX9E, Skip if key
        case 0x9E:
            if(keyboard.test(V[instr.X])){
                PC += 2;
            }
        break;
        // EXA1: Skip if NOT key
        case 0xA1:
            if(!keyboard.test(V[instr.X])){
                PC += 2;
            }
        break;
        default:
            LOGLN("Unreachable: instruction 0xE{:03X}", instr.NNN);
            std::unreachable();
    }
}

void Chip8::handle_F_instr(const instruction_t& instr){
    switch(instr.NN){
        // FX07, reads delay timer and stores it into V[X]
        case 0x7:
            V[instr.X] = delay_timer;
        break;
        // FX0A, wait until a key is pressed and store it in V[X]
        case 0xA:
            for(int i = 0; i < keyboard.size(); ++i){
                if(keyboard.test(i)){
                    V[instr.X] = i;
                    return;
                }
            }
            // trick to keep waiting while no keys are being pressed
            PC -= 2;

        break;
        // FX15, sets delay timer to V[X]
        case 0x15:
            delay_timer = V[instr.X];
        break;
        // FX18, sets sound timer to V[X]
        case 0x18:
            sound_timer = V[instr.X];
        break;
        // FX1E, add V[X] to I
        case 0x1E:
            I += V[instr.X];
            // not std behavior but good to have
            //V[0xF] = (I > 0xFFF);
        break;
        // FX29, set I to the beginning of the system font char stored in VX
        case 0x29:
            // system font starts at ram[0]
            I = ram[V[instr.X] & 0xF];
        break;
        // FX33, convert V[X] to decimal and store the result
        // (always 3 digits) into ram[I], ram[I+1], ram[I+2]
        case 0x33:
            ram[I + 2] = V[instr.X] % 10;
            ram[I + 1] = (V[instr.X] / 10) % 10;
            ram[I] = (V[instr.X] / 100) % 10;
        break;
        // FX55, store registers [V[0], V[X]] to [ram[I], ram[I + X]]
        case 0x55:
            for(int i = 0; i <= instr.X; ++i){
                ram[I + i] = V[i];
            }
            if(FX55_FX65_modify_I){
                I += instr.X + 1;
            }
        break;
        // FX65, load registers [V[0], V[X]] from [ram[I], ram[I + X]]
        case 0x65:
            for(int i = 0; i <= instr.X; ++i){
                V[i] = ram[I + i];
            }
            if(FX55_FX65_modify_I){
                I += instr.X + 1;
            }
        break;
    }
}

void Chip8::cpu_next_instr(){
    LOGLN(
        "CHIP8 internal state:\n\tPC: 0x{:04X} -  "
        "I: 0x{:04X}",
        PC,
        I
    );
    for(int i = 0; i < 16; i += 4){
        LOG("\tV{:0X}: 0x{:02X} - ", i, V[i]);
        LOG("V{:0X}: 0x{:02X} - ", i+1, V[i+1]);
        LOG("V{:0X}: 0x{:02X} - ", i+2, V[i+2]);
        LOGLN("V{:0X}: 0x{:02X}", i+3, V[i+3]);
    }

    // Fetch
    uint16_t tmp = ram[PC++] << 8;
    tmp |= ram[PC++];

    LOGLN("Current instruction: 0x{:0X}", tmp);

    // Decode
    instruction_t instr(tmp);

    // Execute
    switch(tmp >> 12){
        case 0x0: handle_0_instr(instr); break;
        case 0x1: handle_1_instr(instr); break;
        case 0x2: handle_2_instr(instr); break;
        case 0x3: handle_3_instr(instr); break;
        case 0x4: handle_4_instr(instr); break;
        case 0x5: handle_5_instr(instr); break;
        case 0x6: handle_6_instr(instr); break;
        case 0x7: handle_7_instr(instr); break;
        case 0x8: handle_8_instr(instr); break;
        case 0x9: handle_9_instr(instr); break;
        case 0xA: handle_A_instr(instr); break;
        case 0xB: handle_B_instr(instr); break;
        case 0xC: handle_C_instr(instr); break;
        case 0xD: handle_D_instr(instr); break;
        case 0xE: handle_E_instr(instr); break;
        case 0xF: handle_F_instr(instr); break;
    }
}

const std::bitset<Chip8::SCREEN_SIZE>& Chip8::get_screen() const{
    return screen;
}

uint8_t Chip8::get_delay_timer() const{
    return delay_timer;
}

uint8_t Chip8::get_sound_timer() const{
    return sound_timer;
}

void Chip8::decrement_delay_timer(){
    --delay_timer;
}

void Chip8::decrement_sound_timer(){
    --sound_timer;
}