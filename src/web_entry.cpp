// WebAssembly entry point of the solver (Emscripten + embind).
//
// The module runs inside a dedicated web worker (web/worker.js): the search
// threads spawn as nested pthread workers under it. All blocking entry
// points (initialize/run/solve_position) start a detached pthread and
// return immediately - the worker polls the event queue meanwhile, so no
// javascript is ever called from a pthread (embind is not thread safe).
//
// The precomputation is generated into memory on every start (no_file_io):
// the browser persists nothing.

#include <unistd.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <emscripten/atomic.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

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


namespace {

bool initialized = false;
bool init_started = false;

// --- event queue (written from pthreads, drained by the worker) ----------

struct WebEvent {
    std::string kind;   // "stage" | "ready" | "scramble" | "solution" | "error"
    std::string text;
    uint32_t run_idx = 0;
    uint8_t depth = 0;
    std::vector<uint8_t> moves;
};

std::mutex event_mutex;
std::vector<WebEvent> event_queue;

void PushEvent (WebEvent event) {
    std::lock_guard<std::mutex> lock(event_mutex);
    event_queue.push_back(std::move(event));
}

// the search event callback: only queues (called from the search threads).
// Custom solves report run_idx 0 internally - offset them into a distinct
// id space (the app identifies custom runs by runIdx >= kCustomRunOffset).
constexpr uint32_t kCustomRunOffset = 1000000;

SearchEventCallback QueueCallback (uint32_t run_offset = 0) {
    return [run_offset](const SearchEvent& event) {
        WebEvent web_event;
        web_event.kind = event.kind == SearchEventKind::kScramble ? "scramble" : "solution";
        web_event.run_idx = uint32_t(event.run_idx) + run_offset;
        web_event.depth = event.depth;
        web_event.moves = event.moves;
        PushEvent(std::move(web_event));
    };
}

// --- the search job queue -------------------------------------------------
//
// Searches run strictly one after another: the search globals (the
// transposition tables, the leaf queues, the start_work/done_work barriers)
// are shared, so two concurrent SearchManagers would corrupt each other.
// run()/solve_position() push a job and return immediately; one persistent
// worker pthread executes them in order (the idle wait is a futex block, not
// a spin - see emscripten_barrier.hpp).

struct SearchJob {
    bool custom = false;
    State state{};      // custom solves only
};

std::mutex job_mutex;
std::vector<SearchJob> job_queue;
std::atomic<uint32_t> job_wakeup{0};    // futex word: pending jobs
bool search_worker_started = false;

void SearchWorkerLoop () {
    while (true) {
        while (job_wakeup.load(std::memory_order_acquire) == 0) {
            emscripten_atomic_wait_u32((void*)&job_wakeup, 0, -1);
        }
        job_wakeup.fetch_sub(1, std::memory_order_acq_rel);
        SearchJob job;
        {
            std::lock_guard<std::mutex> lock(job_mutex);
            job = std::move(job_queue.front());
            job_queue.erase(job_queue.begin());
        }
        if (job.custom) {
            SearchManagerCustom(job.state, QueueCallback(kCustomRunOffset));
        } else {
            SearchManager(QueueCallback());
        }
    }
}

void EnqueueSearch (SearchJob job) {
    {
        std::lock_guard<std::mutex> lock(job_mutex);
        job_queue.push_back(std::move(job));
    }
    if (!search_worker_started) {
        search_worker_started = true;
        std::thread(SearchWorkerLoop).detach();
    }
    job_wakeup.fetch_add(1, std::memory_order_acq_rel);
    emscripten_atomic_notify((void*)&job_wakeup, INT32_MAX);
}


// --- initialization -------------------------------------------------------

void Initialize (const std::vector<std::string>& arguments) {
    if (initialized || init_started) {
        LOG_WARNING("puppetweb is already initialized");
        return;
    }
    init_started = true;

    // argv for the settings parser
    std::vector<std::string> argument_storage;
    argument_storage.push_back("puppetweb");
    argument_storage.insert(argument_storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (std::string& argument : argument_storage) {
        argv.push_back(argument.data());
    }

    // settings initialization (reset of the getopt state)
    optind = 0;
    no_file_io = true;
    Settings(int(argv.size()), argv.data());
    FixAllocatorThreshold();

    // precomputation (generated into memory, no files)
    RotationInit();
    edge::Init();
    corner::Init();
    tablebase::Init();
    DuplicateRotations::Initialize();
    LOG_INFO("Loaded Precomputation");

    transposition_table::Init();
    LOG_INFO("Transposition Table Initialized");

    initialized = true;
}

// emscripten 6 embind marshals std::vector parameters as vector instances -
// javascript arrays are read through emscripten::val instead
std::vector<std::string> ToStringVector (const emscripten::val& array) {
    std::vector<std::string> result;
    const size_t length = array["length"].as<size_t>();
    result.reserve(length);
    for (size_t i = 0; i < length; i++) {
        result.push_back(array[i].as<std::string>());
    }
    return result;
}

// one piece placement from javascript as a flat int array:
// [slot xyz, piece home xyz, then per sticker (home dir xyz, current dir xyz)]
std::vector<input::PiecePlacement> ToPlacements (const emscripten::val& pieces) {
    std::vector<input::PiecePlacement> placements;
    const size_t num_pieces = pieces["length"].as<size_t>();
    placements.reserve(num_pieces);
    for (size_t p = 0; p < num_pieces; p++) {
        const emscripten::val piece = pieces[p];
        const size_t length = piece["length"].as<size_t>();
        if (length < 6 || (length - 6) % 6 != 0) {
            throw std::runtime_error("invalid placement: expected [slot(3), piece(3), sticker dirs(6 each)]");
        }
        std::vector<int> values;
        values.reserve(length);
        for (size_t i = 0; i < length; i++) {
            values.push_back(piece[i].as<int>());
        }
        input::PiecePlacement placement;
        placement.slot = {values[0], values[1], values[2]};
        placement.piece = {values[3], values[4], values[5]};
        for (size_t i = 6; i < length; i += 6) {
            placement.sticker_home_dirs.push_back({values[i+0], values[i+1], values[i+2]});
            placement.sticker_cur_dirs.push_back({values[i+3], values[i+4], values[i+5]});
        }
        placements.push_back(std::move(placement));
    }
    return placements;
}

std::vector<uint8_t> ToMoveVector (const emscripten::val& array) {
    std::vector<uint8_t> moves;
    const size_t length = array["length"].as<size_t>();
    moves.reserve(length);
    for (size_t i = 0; i < length; i++) {
        moves.push_back(array[i].as<uint8_t>());
    }
    return moves;
}

struct ConvertResult {
    bool ok = false;
    std::vector<uint32_t> state;
    int error = 0;
    std::string message;
};

struct RotRepWeb {
    std::string name;
    uint8_t index = 0;
    std::vector<int> matrix;
    int activate = 0;
};

} // namespace


// starts the initialization on a pthread (generation takes minutes - the
// stage progress arrives via poll_events)
void InitializeAsync (const emscripten::val& arguments) {
    if (init_started) {
        throw std::runtime_error("puppetweb is already initialized");
    }

    // the stage progress of the precomputation (fires on the init thread)
    progress_hook = [](const std::string& stage) {
        WebEvent event;
        event.kind = "stage";
        event.text = stage;
        PushEvent(std::move(event));
    };

    std::thread([arguments = ToStringVector(arguments)]() {
        Initialize(arguments);
        WebEvent event;
        event.kind = initialized ? "ready" : "error";
        event.text = initialized ? "initialization complete" : "initialization failed";
        PushEvent(std::move(event));
    }).detach();
}


// runs the scramble searches (num_runs from the settings); the events of
// every run arrive via poll_events (queued - a still running search is
// finished first)
void RunAsync () {
    if (!initialized) {
        throw std::runtime_error("puppetweb is not initialized - call initialize first");
    }
    EnqueueSearch(SearchJob{});
}


// converts a user input cube into the internal state and checks solvability
// (synchronous - the validation feedback must be immediate)
ConvertResult ConvertPositionBinding (const emscripten::val& corners,
                                      const emscripten::val& edges) {
    if (!initialized) {
        throw std::runtime_error("puppetweb is not initialized - call initialize first");
    }
    input::PositionResult result = input::ConvertPosition(ToPlacements(corners), ToPlacements(edges));
    ConvertResult web_result;
    web_result.ok = result.ok;
    web_result.error = int(result.error);
    web_result.message = result.message;
    if (result.ok) {
        web_result.state = {result.state.edge_pos, result.state.corner_pos, result.state.corner_orient,
                            result.state.edge_orient, result.state.edge_sym};
    }
    return web_result;
}


// solves a user input position (converts again internally - cheap); throws
// synchronously on an invalid position, the solution arrives via poll_events
void SolvePositionAsync (const emscripten::val& corners,
                         const emscripten::val& edges) {
    if (!initialized) {
        throw std::runtime_error("puppetweb is not initialized - call initialize first");
    }
    input::PositionResult result = input::ConvertPosition(ToPlacements(corners), ToPlacements(edges));
    if (!result.ok) {
        throw std::runtime_error("invalid position: " + result.message);
    }
    SearchJob job;
    job.custom = true;
    job.state = result.state;
    EnqueueSearch(std::move(job));
}


// the internal state reached from a start state (solved if empty) after
// applying the moves (rotation indices) - test helper
std::vector<uint32_t> StateAfterMoves (const emscripten::val& moves_val,
                                       const emscripten::val& start_val) {
    if (!initialized) {
        throw std::runtime_error("puppetweb is not initialized - call initialize first");
    }
    State state = kSolvedState;
    if (start_val["length"].as<size_t>() == 5) {
        std::array<uint32_t, 5> fields{};
        for (size_t i = 0; i < 5; i++) {
            fields[i] = start_val[i].as<uint32_t>();
        }
        state = State{fields[0], uint16_t(fields[1]), uint16_t(fields[2]),
                      uint16_t(fields[3]), uint8_t(fields[4])};
    }
    for (uint8_t rot : ToMoveVector(moves_val)) {
        corner::Rotate(state.corner_pos, state.corner_orient, rot);
        edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rot);
    }
    return {state.edge_pos, state.corner_pos, state.corner_orient, state.edge_orient, state.edge_sym};
}


// rotation representations (name, matrix, activate) shared with the search
std::vector<RotRepWeb> RotationReps () {
    RotationInit();
    std::vector<RotRepWeb> rotation_reps(kNumRot);
    for (uint8_t i = 0; i < kNumRot; i++) {
        rotation_reps[i].name = idx_to_rot_rep[i].name;
        rotation_reps[i].index = idx_to_rot_rep[i].index;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                rotation_reps[i].matrix.push_back(idx_to_rot_rep[i].matrix[row][col]);
            }
        }
        rotation_reps[i].activate = idx_to_rot_rep[i].activate;
    }
    return rotation_reps;
}


// drains the event queue (called by the worker on its main thread)
std::vector<WebEvent> PollEvents () {
    std::lock_guard<std::mutex> lock(event_mutex);
    std::vector<WebEvent> events;
    events.swap(event_queue);
    return events;
}


int NumThreads () {
    return Settings::GetNumThreads();
}


EMSCRIPTEN_BINDINGS(puppetweb) {
    emscripten::value_object<WebEvent>("WebEvent")
        .field("kind", &WebEvent::kind)
        .field("text", &WebEvent::text)
        .field("runIdx", &WebEvent::run_idx)
        .field("depth", &WebEvent::depth)
        .field("moves", &WebEvent::moves);

    emscripten::value_object<ConvertResult>("ConvertResult")
        .field("ok", &ConvertResult::ok)
        .field("state", &ConvertResult::state)
        .field("error", &ConvertResult::error)
        .field("message", &ConvertResult::message);

    emscripten::value_object<RotRepWeb>("RotRep")
        .field("name", &RotRepWeb::name)
        .field("index", &RotRepWeb::index)
        .field("matrix", &RotRepWeb::matrix)
        .field("activate", &RotRepWeb::activate);

    emscripten::register_vector<uint8_t>("vector<uint8_t>");
    emscripten::register_vector<uint32_t>("vector<uint32_t>");
    emscripten::register_vector<int>("vector<int>");
    emscripten::register_vector<WebEvent>("vector<WebEvent>");
    emscripten::register_vector<RotRepWeb>("vector<RotRepWeb>");

    emscripten::function("initialize", &InitializeAsync);
    emscripten::function("run", &RunAsync);
    emscripten::function("convert_position", &ConvertPositionBinding);
    emscripten::function("solve_position", &SolvePositionAsync);
    emscripten::function("state_after_moves", &StateAfterMoves);
    emscripten::function("rotation_reps", &RotationReps);
    emscripten::function("poll_events", &PollEvents);
    emscripten::function("num_threads", &NumThreads);
}
