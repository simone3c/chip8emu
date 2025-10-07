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

//#define DEBUG

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
    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;

    std::bitset<SCREEN_SIZE> screen;
    std::bitset<KEYBOARD_SIZE> keyboard;

    void handle_0_instr(const instruction_t& instr);
    void handle_1_instr(const instruction_t& instr);
    void handle_2_instr(const instruction_t& instr);
    void handle_3_instr(const instruction_t& instr);
    void handle_4_instr(const instruction_t& instr);
    void handle_5_instr(const instruction_t& instr);
    void handle_6_instr(const instruction_t& instr);
    void handle_7_instr(const instruction_t& instr);
    void handle_8_instr(const instruction_t& instr);
    void handle_9_instr(const instruction_t& instr);
    void handle_A_instr(const instruction_t& instr);
    void handle_B_instr(const instruction_t& instr);
    void handle_C_instr(const instruction_t& instr);
    void handle_D_instr(const instruction_t& instr);
    void handle_E_instr(const instruction_t& instr);
    void handle_F_instr(const instruction_t& instr);

    public:
    void cpu_next_instr();
    void load(const std::vector<uint8_t>& prog);
    const std::bitset<SCREEN_SIZE>& get_screen() const;
    uint8_t get_delay_timer() const;
    uint8_t get_sound_timer() const;
    void decrement_delay_timer();
    void decrement_sound_timer();
};


