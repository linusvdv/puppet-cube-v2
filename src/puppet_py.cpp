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
#include "position_input.hpp"
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

// wraps the python callback such that it can be called from the search:
// the python interpreter lock is acquired for every event and exceptions
// are not allowed to propagate through the search (the search worker
// threads wait on barriers and can not be unwound)
SearchEventCallback MakeTrampoline (const SearchEventCallback& event_callback) {
    return [&event_callback](const SearchEvent& event) {
        py::gil_scoped_acquire acquire;
        try {
            event_callback(event);
        }
        catch (py::error_already_set& error) {
            LOG_WARNING("python event callback raised an exception:", error.what());
            error.discard_as_unraisable("puppetpy event callback");
        }
    };
}

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
            LOG_EXTRA("Create precomputation folder for precomputation");
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

    {
        py::gil_scoped_release release;
        SearchManager(MakeTrampoline(event_callback));
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


// one piece placement as passed from python:
// (slot xyz, piece home xyz, sticker home face dirs, sticker current face dirs)
using PlacementTuple = std::tuple<std::array<int, 3>, std::array<int, 3>,
                                  std::vector<std::array<int, 3>>, std::vector<std::array<int, 3>>>;


std::vector<input::PiecePlacement> ToPlacements (const std::vector<PlacementTuple>& tuples) {
    std::vector<input::PiecePlacement> placements;
    placements.reserve(tuples.size());
    for (const auto& [slot, piece, home_dirs, cur_dirs] : tuples) {
        placements.push_back({slot, piece, home_dirs, cur_dirs});
    }
    return placements;
}


// converts a user input cube (piece placements) into the internal state
// representation and checks that the position is solvable
// returns (ok, state tuple or None, error, message)
py::object ConvertPositionBinding (const std::vector<PlacementTuple>& corners, const std::vector<PlacementTuple>& edges) {
    if (!initialized) {
        throw std::runtime_error("puppetpy is not initialized - call initialize first");
    }
    input::PositionResult result = input::ConvertPosition(ToPlacements(corners), ToPlacements(edges));
    if (!result.ok) {
        return py::make_tuple(false, py::none(), result.error, result.message);
    }
    const State& state = result.state;
    return py::make_tuple(true, py::make_tuple(state.edge_pos, state.corner_pos, state.corner_orient,
                                               state.edge_orient, state.edge_sym),
                          result.error, result.message);
}


// solves a user input position: converts and checks the piece placements,
// then runs the search on the position; the callback receives the (empty)
// scramble event and the solution event of the single run
// throws std::runtime_error if the position is not valid
void SolvePosition (const std::vector<PlacementTuple>& corners, const std::vector<PlacementTuple>& edges,
                    const SearchEventCallback& event_callback) {
    if (!initialized) {
        throw std::runtime_error("puppetpy is not initialized - call initialize first");
    }
    input::PositionResult result = input::ConvertPosition(ToPlacements(corners), ToPlacements(edges));
    if (!result.ok) {
        throw std::runtime_error("invalid position: " + result.message);
    }

    {
        py::gil_scoped_release release;
        SearchManagerCustom(result.state, MakeTrampoline(event_callback));
    }
}


// the internal state reached from a start state (the solved state if none is
// given) after applying the moves (rotation indices); used to test the
// conversion of user input positions
std::array<uint32_t, 5> StateAfterMoves (const std::vector<uint8_t>& moves, const py::object& start_state) {
    if (!initialized) {
        throw std::runtime_error("puppetpy is not initialized - call initialize first");
    }
    State state = kSolvedState;
    if (!start_state.is_none()) {
        std::array<uint32_t, 5> fields = start_state.cast<std::array<uint32_t, 5>>();
        state = State{fields[0], uint16_t(fields[1]), uint16_t(fields[2]), uint16_t(fields[3]), uint8_t(fields[4])};
    }
    for (uint8_t rot : moves) {
        corner::Rotate(state.corner_pos, state.corner_orient, rot);
        edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rot);
    }
    return {state.edge_pos, state.corner_pos, state.corner_orient, state.edge_orient, state.edge_sym};
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

    py::enum_<input::PositionError>(m, "PositionError")
        .value("NONE", input::PositionError::kNone)
        .value("INVALID_INPUT", input::PositionError::kInvalidInput)
        .value("INVALID_SLOT", input::PositionError::kInvalidSlot)
        .value("DUPLICATE_PIECE", input::PositionError::kDuplicatePiece)
        .value("INVALID_ASSIGNMENT", input::PositionError::kInvalidAssignment)
        .value("REFLECTED_CORNER", input::PositionError::kReflectedCorner)
        .value("CORNER_ORIENT_PARITY", input::PositionError::kCornerOrientParity)
        .value("EDGE_ORIENT_PARITY", input::PositionError::kEdgeOrientParity)
        .value("CORNER_UNSOLVABLE", input::PositionError::kCornerUnsolvable)
        .value("PARITY_MISMATCH", input::PositionError::kParityMismatch)
        .value("INTERNAL", input::PositionError::kInternal);

    m.def("initialize", &Initialize, py::arg("arguments"));
    m.def("run", &Run, py::arg("event_callback"));
    m.def("rotation_reps", &RotationReps);
    m.def("convert_position", &ConvertPositionBinding, py::arg("corners"), py::arg("edges"));
    m.def("state_after_moves", &StateAfterMoves, py::arg("moves"), py::arg("start") = py::none());
    m.def("solve_position", &SolvePosition, py::arg("corners"), py::arg("edges"), py::arg("event_callback"));
}
