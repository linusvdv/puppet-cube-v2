#include <filesystem>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "corner.hpp"
#include "duplicate_rotations.hpp"
#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"
#include "transposition_table.hpp"
#include "utils.hpp"

#ifdef USE_CUDA
#include "corner_bridge.hpp"
#include "edge_bridge.hpp"
#include "info_bridge.hpp"
#include "leaf_search_bridge.hpp"
#include "tablebase_bridge.hpp"
#endif


namespace py = pybind11;

namespace {

bool initialized = false;

}


// settings + precomputation initialization (has to be called once before run)
void Initialize (const std::vector<std::string>& arguments) {
    if (initialized) {
        LOG_WARNING("puppetpy is already initialized");
        return;
    }

    // argv for the settings parser
    std::vector<std::string> argument_storage;
    argument_storage.push_back("puppetpy");
    argument_storage.insert(argument_storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (std::string& argument : argument_storage) {
        argv.push_back(argument.data());
    }

    // the initialization is pure c++ (loading ~10 GB of precomputation tables)
    // - release the python interpreter lock such that the gui stays responsive
    py::gil_scoped_release release;

    // settings initialization (reset of the getopt state)
    optind = 0;
    Settings(int(argv.size()), argv.data());
    LOG_MEMORY();

    // precomputation
    if (!std::filesystem::exists(GetFilePath(""))) {
        if (std::filesystem::create_directories(GetFilePath(""))) {
            LOG_ALL("Create precomputation folder for precomputation");
        }
        else {
            LOG_CRITICAL("Failed to create folder for precomputation");
        }
    }
    RotationInit();
    edge::Init();
    corner::Init();
    tablebase::Init();
    DuplicateRotations::Initialize();
    LOG_INFO("Loaded Precomputation");

    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        LOG_EXTRA("Start Edge Precomputation Uploading to Device");
        edge::UploadPrecomputationToDevice();
        LOG_EXTRA("Start Corner Precomputation Uploading to Device");
        corner::UploadPrecomputationToDevice();
        LOG_EXTRA("Start Tablebase Precomputation Uploading to Device");
        tablebase::UploadPrecomputationToDevice();
        LOG_INFO("Precomputation Uploaded to Device");
        LOG_MEMORY();
    }
    #endif // USE_CUDA

    LOG_EXTRA("Start Transposition Table Initialization");
    transposition_table::Init();
    LOG_INFO("Transposition Table Initialized");
    LOG_MEMORY();

    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        CudaConstMemInitialize();
        LOG_INFO("Cuda Constant Memory Initialized");
        LOG_MEMORY();
    }
    #endif // USE_CUDA

    initialized = true;
}


// runs the search manager, the python callback is called with the scramble and the solution of every run
// the python interpreter lock is released during the search and acquired again for the callbacks
void Run (const SearchEventCallback& event_callback) {
    if (!initialized) {
        throw std::runtime_error("puppetpy is not initialized - call initialize first");
    }

    auto trampoline = [&event_callback](const SearchEvent& event) {
        py::gil_scoped_acquire acquire;
        try {
            event_callback(event);
        }
        catch (py::error_already_set& error) {
            // exceptions are not allowed to propagate through the search
            // (the search worker threads wait on barriers and can not be unwound)
            LOG_WARNING("python event callback raised an exception:", error.what());
            error.discard_as_unraisable("puppetpy event callback");
        }
    };

    {
        py::gil_scoped_release release;
        SearchManager(trampoline);
    }
}


// rotation representations (name, matrix, activate) shared with the search
std::vector<RotRep> RotationReps () {
    RotationInit();
    std::vector<RotRep> rotation_reps(kNumRot);
    for (uint8_t i = 0; i < kNumRot; i++) {
        rotation_reps[i] = idx_to_rot_rep[i];
    }
    return rotation_reps;
}


PYBIND11_MODULE(puppetpy, m) {
    m.doc() = "Puppet Cube V2 solver bindings";

    py::enum_<SearchEventKind>(m, "SearchEventKind")
        .value("SCRAMBLE", SearchEventKind::kScramble)
        .value("SOLUTION", SearchEventKind::kSolution);

    py::class_<SearchEvent>(m, "SearchEvent")
        .def_readonly("kind", &SearchEvent::kind)
        .def_readonly("run_idx", &SearchEvent::run_idx)
        .def_readonly("depth", &SearchEvent::depth)
        .def_readonly("moves", &SearchEvent::moves);

    py::class_<RotRep>(m, "RotRep")
        .def_readonly("name", &RotRep::name)
        .def_readonly("index", &RotRep::index)
        .def_readonly("matrix", &RotRep::matrix)
        .def_readonly("activate", &RotRep::activate);

    m.def("initialize", &Initialize, py::arg("arguments"));
    m.def("run", &Run, py::arg("event_callback"));
    m.def("rotation_reps", &RotationReps);
}
