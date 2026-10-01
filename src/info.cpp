#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "logger.hpp"


#if defined(_WIN32)
    // ---------- Windows ----------
    #include <windows.h>
#elif defined(__linux__)
    // ---------- Linux ----------
    #include <fstream>
    #include <iostream>
#elif defined(__APPLE__)
    // ---------- macOS ----------
    #include <sys/sysctl.h>
#endif


std::vector<std::string> GetCpuName() {
    std::vector<std::string> result;

#if defined(_WIN32)
    // -------------------- Windows --------------------
    DWORD index = 0;
    char key_path[256];

    while (true) {
        // Try each CentralProcessor\N entry
        sprintf(key_path, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%lu", index);

        HKEY h_key;
        if (RegOpenKeyA(HKEY_LOCAL_MACHINE, key_path, &h_Key) != ERROR_SUCCESS) {
            break;  // no more CPUs listed
        }

        char buffer[256];
        DWORD buffer_size = sizeof(buffer);
        if (RegGetValueA(
                HKEY_LOCAL_MACHINE,
                key_path,
                "ProcessorNameString",
                RRF_RT_ANY,
                NULL,
                &buffer,
                &buffer_size
            ) == ERROR_SUCCESS)
        {
            result.emplace_back(buffer);
        }

        RegCloseKey(h_Key);
        index++;
    }

    // Remove duplicates (multiple entries for same socket)
    {
        std::unordered_set<std::string> unique(result.begin(), result.end());
        result.assign(unique.begin(), unique.end());
    }

#elif defined(__linux__)
    // -------------------- Linux --------------------
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    std::vector<std::string> socket_models;
    std::string current_model;
    std::unordered_map<int,std::string> socket_map;

    int socket_id = -1;

    while (std::getline(cpuinfo, line)) {
        if (line.rfind("physical id", 0) == 0) {
            socket_id = std::stoi(line.substr(line.find(":") + 2));
        }
        if (line.rfind("model name", 0) == 0) {
            std::string model = line.substr(line.find(":") + 2);
            if (socket_id != -1) {
                socket_map[socket_id] = model;
            }
        }
    }

    for (auto& socket : socket_map) {
        result.push_back(socket.second);
    }

#elif defined(__APPLE__)
    // -------------------- macOS --------------------
    // macOS does not expose multiple sockets separately.
    // It always presents a unified CPU brand string.
    char buffer[256];
    size_t size = sizeof(buffer);
    if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, NULL, 0) == 0) {
        result.push_back(buffer);
    }

#else
    result.push_back("Unsupported platform");
#endif

    return result;
}


void GetHostInfo() {
    std::vector<std::string> cpu_names = GetCpuName();
    LOG_ALL("Number CPUs:", cpu_names.size());
    for (size_t i = 0; i < cpu_names.size(); i++) {
        LOG_ALL("CPU", SkipSpace(i), ":", cpu_names[i]);
    }
}
