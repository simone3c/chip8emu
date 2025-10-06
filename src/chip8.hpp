#include <array>
#include <bitset>
#include <stack>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <utility>
#include <random>
#include <print>
#include <chrono>
#include <thread>

#define DEBUG

#ifdef DEBUG
#define LOG(...) std::print(stderr, __VA_ARGS__)
#define LOGLN(...) std::println(stderr, __VA_ARGS__)
#else
#define LOG(...) ;
#define LOGLN(...) ;
#endif

class Chip8{
    /*
        https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

        https://riv.dev/emulating-a-computer-part-4/
    */

    // emulator config
    // TODO set them in constructor and take them from CLI (GUI if it will exist)
    bool copy_vy_to_vx_in_shift = false;
    bool make_BNNN_into_BXNN = false;
    bool FX55_FX65_modify_I = false;
    int ips = 700; // instruction per second. 700 should be good
    int refresh_rate = 60; // FPS

    struct instruction_t{
        uint32_t X: 4;
        uint32_t Y: 4;
        uint32_t N: 4;
        uint32_t NN: 8;
        // use NNN also to decode the instruction, see handle_x_instr()
        uint32_t NNN: 12;

        instruction_t(uint16_t n){
            X = (n >> 8) & 0xF;
            Y = (n >> 4) & 0xF;
            N = n & 0xF;
            NN = n & 0xFF;
            NNN = n & 0xFFF;
        };
    };

    static constexpr auto RAM_SIZE = 4096; // bytes
    static constexpr auto MAX_PROG_SIZE = 4096 - 0x200; // bytes
    static constexpr auto GPREG_NUM = 16;
    static constexpr uint8_t FONT_START_ADDR = 0; // sys fonts stored here in ram
    static constexpr uint16_t PC_RESET_VALUE = 0x200;
    static constexpr auto SCREEN_SIZE = 64 * 32; // 32 rows by 64 columns
    static constexpr auto KEYBOARD_SIZE = 16; // keys go from '0' to 'F'

    std::array<uint8_t, RAM_SIZE> ram{
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    uint16_t PC = PC_RESET_VALUE;
    uint16_t I;
    std::array<uint8_t, GPREG_NUM> V;
    std::stack<uint16_t> stack;
    uint8_t delay_timer;
    uint8_t sound_timer;

    std::bitset<SCREEN_SIZE> screen;
    std::bitset<KEYBOARD_SIZE> keyboard;

    constexpr void handle_0_instr(const instruction_t& instr){
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

    constexpr void handle_1_instr(const instruction_t& instr){
        // only 1NNN here, JMP
        PC = instr.NNN;
    }

    constexpr void handle_2_instr(const instruction_t& instr){
        // only 2NNN, jump to subroutine
        stack.push(PC);
        PC = instr.NNN;
    }

    constexpr void handle_3_instr(const instruction_t& instr){
        // only 3XNN, skip conditionally
        if(V[instr.X] == instr.NN){
            PC += 2;
        }
    }

    constexpr void handle_4_instr(const instruction_t& instr){
        // only 4XNN, skip conditionally
        if(V[instr.X] != instr.NN){
            PC += 2;
        }
    }

    constexpr void handle_5_instr(const instruction_t& instr){
        // only 5XY0, skip conditionally
        if(V[instr.X] == V[instr.Y]){
            PC += 2;
        }
    }

    constexpr void handle_6_instr(const instruction_t& instr){
        // only 6XNN, set register
        V[instr.X] = instr.NN;
    }

    constexpr void handle_7_instr(const instruction_t& instr){
        // only 7XNN, add to register
        V[instr.X] += instr.NN;
    }

    constexpr void handle_8_instr(const instruction_t& instr){
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

    constexpr void handle_9_instr(const instruction_t& instr){
        // only 9XY0, skip conditionally
        if(V[instr.X] != V[instr.Y]){
            PC += 2;
        }
    }

    constexpr void handle_A_instr(const instruction_t& instr){
        // only ANNN, I = NNN
        I = instr.NNN;
    }

    constexpr void handle_B_instr(const instruction_t& instr){
        // only BNNN, PC = NNN + V0 (default)
        // or BXNN, PC = V[X] + XNN
        uint8_t reg = 0;
        if(make_BNNN_into_BXNN){
            reg = instr.X;
        }

        PC = V[reg] + instr.NNN;
    }

    constexpr void handle_C_instr(const instruction_t& instr){
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> distrib(0, 255);

        // only CXNN, VX = rand() & NN
        V[instr.X] = distrib(gen) & instr.NN;
    }

    constexpr void handle_D_instr(const instruction_t& instr){
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

    constexpr void handle_E_instr(const instruction_t& instr){
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

    void handle_F_instr(const instruction_t& instr){
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
                V[0xF] = (I > 0xFFF);
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


    public:
    void cpu_next_instr(){
        // LOGLN(
        //     "CHIP8 internal state:\n\tPC: 0x{:04X} -  "
        //     "I: 0x{:04X}",
        //     PC,
        //     I
        // );
        // for(int i = 0; i < 16; i += 4){
        //     LOG("\tV{:0X}: 0x{:02X} - ", i, V[i]);
        //     LOG("V{:0X}: 0x{:02X} - ", i+1, V[i+1]);
        //     LOG("V{:0X}: 0x{:02X} - ", i+2, V[i+2]);
        //     LOGLN("V{:0X}: 0x{:02X}", i+3, V[i+3]);
        // }

        // Fetch
        uint16_t tmp = ram[PC++] << 8;
        tmp |= ram[PC++];

        // std::println(
        //     stderr,
        //     "Current instruction: 0x{:0X}",
        //     tmp
        // );

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
    void load(const std::vector<uint8_t>& prog);
    const auto& get_screen() const{
        return screen;

    }
    
    uint8_t get_delay_timer() const{
        return delay_timer;
    }

    uint8_t get_sound_timer() const{
        return sound_timer;
    }

    void decrement_delay_timer(){
        --delay_timer;
    }

    void decrement_sound_timer(){
        --sound_timer;
    }
};


