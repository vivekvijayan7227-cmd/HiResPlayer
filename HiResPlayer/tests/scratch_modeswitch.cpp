#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include "hires_audio_api.h"

int main() {
    std::cout << "Starting scratch_modeswitch test..." << std::endl;
    HP_Handle engine = hp_create();
    if (!engine) {
        std::cout << "hp_create failed" << std::endl;
        return 1;
    }

    const wchar_t* fixture = L"d:\\AudioPlayer\\HiResPlayer\\tests\\fixtures\\test_16bit_44k.flac";
    HP_Result res = hp_open_flac(engine, fixture);
    std::cout << "hp_open_flac: " << res << std::endl;

    res = hp_set_mode(engine, HP_SHARED);
    std::cout << "hp_set_mode(SHARED): " << res << std::endl;

    res = hp_play(engine);
    std::cout << "hp_play: " << res << std::endl;

    std::cout << "Sleeping 300ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::cout << "Switching to EXCLUSIVE while playing..." << std::endl;
    res = hp_set_mode(engine, HP_EXCLUSIVE);
    std::cout << "hp_set_mode(EXCLUSIVE): " << res << std::endl;

    std::cout << "Sleeping 300ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::cout << "Switching back to SHARED while playing..." << std::endl;
    res = hp_set_mode(engine, HP_SHARED);
    std::cout << "hp_set_mode(SHARED): " << res << std::endl;

    std::cout << "Sleeping 300ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    HP_StatusV1 st = {};
    hp_get_status(engine, &st);
    std::cout << "Rendered samples: " << st.rendered_source_samples << std::endl;

    hp_stop(engine);
    hp_destroy(engine);
    std::cout << "SUCCESS! Completed without crash." << std::endl;
    return 0;
}
