// Standalone regression tests. /Zs evaluates the static_assert admission tests
// without emitting a binary; running main requires a separately authorized build.
#include "SkyUltra/BridgeEventQueue.h"
#include "SkyUltra/RuntimeSettings.h"
#include <array>
#include <atomic>
#include <cstdlib>
#define CHECK(...) do { if (!(__VA_ARGS__)) std::abort(); } while (false)
#include <limits>
#include <thread>

using Queue = skyultra::BridgeEventQueue;
static_assert(Queue::kMaxPendingEvents == 1024);
static_assert(Queue::Fits(1023, 1023, 1));
static_assert(!Queue::Fits(1024, 1024, 1));
static_assert(Queue::Fits(0, 0, Queue::kMaxPendingBytes));
static_assert(!Queue::Fits(0, 0, Queue::kMaxPendingBytes + 1));
static_assert(!Queue::Fits(0, std::numeric_limits<std::size_t>::max(), 1));
static_assert(!Queue::Fits(16, 100, 1, 0, true));
static_assert(Queue::Fits(16, 100, 1, 50, true, true));
static_assert(!Queue::Fits(16, 100, Queue::kMaxControlBytes, 0, true, true));

int main()
{
    {
        const auto defaults = skyultra::DefaultRuntimeSettings();
        nlohmann::json parsed;
        std::string error;
        CHECK(skyultra::ParseRuntimeSettings({{"mainWidth", 1920}, {"renderMode", "cpu"}}, defaults, parsed, error));
        CHECK(parsed["mainWidth"] == 1920 && parsed["mainHeight"] == 0 && parsed["renderMode"] == "cpu");
        CHECK(!skyultra::ParseRuntimeSettings({{"mainWidth", -1}}, defaults, parsed, error));
        CHECK(!skyultra::ParseRuntimeSettings({{"popupWidth", 127}}, defaults, parsed, error));
        CHECK(!skyultra::ParseRuntimeSettings({{"mainHeight", 4097}}, defaults, parsed, error));
        CHECK(!skyultra::ParseRuntimeSettings({{"mainWidth", 1.5}}, defaults, parsed, error));
        CHECK(!skyultra::ParseRuntimeSettings({{"renderMode", "invalid"}}, defaults, parsed, error));
        CHECK(!skyultra::ParseRuntimeSettings({{"hotReload", 1}}, defaults, parsed, error));
        CHECK(!skyultra::ParseRuntimeSettings({{"maxFps", 60}}, defaults, parsed, error));
        CHECK(defaults == skyultra::DefaultRuntimeSettings());
    }
    {
        Queue queue;
        for (int i = 0; i < 1024; ++i)
            CHECK(queue.Push("skyos.invokeChunk", std::to_string(i)) == Queue::Result::Accepted);
        CHECK(queue.Push("overflow", "null") == Queue::Result::Full);
        CHECK(!queue.Take(0)); // Accepted during creation, before engine attachment.
        const auto consumer = queue.Attach([] {});
        CHECK(queue.Push("skyultra.visibility", "true") == Queue::Result::Accepted);
        CHECK(queue.Push("skyultra.visibility", "false") == Queue::Result::Accepted);
        CHECK(queue.Snapshot().coalesced == 1);
        std::size_t expected{};
        bool visibility{};
        while (auto batch = queue.Take(consumer)) {
            CHECK(batch->events.size() <= Queue::kBatchEvents);
            for (const auto& event : batch->events) {
                if (event.control) { CHECK(!visibility); CHECK(event.payload_json == "false"); visibility = true; }
                else { CHECK(std::stoul(event.payload_json) == expected++); }
            }
            batch->executed = true;
        }
        CHECK(visibility && expected == 1024);
        CHECK(queue.Snapshot().pending == 0 && queue.Snapshot().bytes == 0);
    }
    {
        Queue queue;
        const auto first = queue.Attach([] {});
        for (int i = 0; i < 1024; ++i) CHECK(queue.Push("ordered", "1") == Queue::Result::Accepted);
        auto flight = queue.Take(first);
        CHECK(queue.Snapshot().pending == 1024 && queue.Snapshot().in_flight == 8);
        CHECK(queue.Push("overflow", "1") == Queue::Result::Full); // Flight still reserves memory/count.
        const auto second = queue.Attach([] {});
        CHECK(!queue.Pending(first) && !queue.Take(first));
        CHECK(queue.Pending(second));
        queue.Close();
        CHECK(queue.Snapshot().pending == flight->events.size());
        CHECK(queue.Push("closed", "1") == Queue::Result::Closed);
        flight.reset();
        CHECK(queue.Snapshot().pending == 0 && queue.Snapshot().bytes == 0);
        CHECK(queue.Snapshot().cancelled == 1024);
    }
    {
        Queue responses(false);
        const auto consumer = responses.Attach([] { throw 1; });
        CHECK(responses.Push("skyultra.visibility", "1") == Queue::Result::Accepted);
        CHECK(responses.Push("skyultra.visibility", "2") == Queue::Result::Accepted);
        auto batch = responses.Take(consumer);
        CHECK(batch->events.size() == 2 && responses.Snapshot().coalesced == 0);
        batch->executed = true;
    }
    {
        Queue queue;
        const auto consumer = queue.Attach([] {});
        std::array<std::thread, 4> producers;
        for (std::size_t p = 0; p < producers.size(); ++p) producers[p] = std::thread([&, p] {
            for (int i = 0; i < 128; ++i)
                CHECK(queue.Push(std::to_string(p), std::to_string(i)) == Queue::Result::Accepted);
        });
        for (auto& producer : producers) producer.join();
        std::array<int, 4> expected{};
        while (auto batch = queue.Take(consumer)) {
            for (const auto& event : batch->events) CHECK(std::stoi(event.payload_json) == expected[std::stoul(event.name)]++);
            batch->executed = true;
        }
        for (const auto count : expected) CHECK(count == 128);
        CHECK(queue.Snapshot().delivered == 512 && queue.Snapshot().bytes == 0);
    }
}
