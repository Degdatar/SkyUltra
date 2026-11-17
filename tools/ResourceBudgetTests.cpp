// /Zs evaluates the constexpr production admission/retry cases. The runtime
// scenarios in main require a separately authorized native test build.
#include "SkyUltra/BridgeEventQueue.h"
#include "SkyUltra/CefRecoveryPolicy.h"
#include <array>
#include <cstdlib>
#include <limits>
#include <vector>
#define CHECK(...) do { if (!(__VA_ARGS__)) std::abort(); } while (false)
using Budget = skyultra::ModResourceBudget;
using Queue = skyultra::BridgeEventQueue;
static_assert(Budget::Fits(4095, 1, Budget::kMaxMessages));
static_assert(!Budget::Fits(4096, 1, Budget::kMaxMessages));
static_assert(!Budget::Fits(std::numeric_limits<std::size_t>::max(), 1, Budget::kMaxMessageBytes));
static_assert(Budget::SurfaceBytes(4096,4096) == 64U * 1024U * 1024U);
static_assert(Budget::SurfaceBytes(0,1080) > Budget::kMaxSurfaceBytes);
static_assert(Budget::SurfaceBytes(100000,100000) > Budget::kMaxSurfaceBytes);
static_assert([] {
    skyultra::CefRecoveryPolicy policy;
    return policy.NextDelayMs(0) == 250 && policy.NextDelayMs(100) == 1000 &&
        policy.NextDelayMs(200) == 4000 && policy.NextDelayMs(300) == 0 && policy.NextDelayMs(60000) == 250;
}());

int main()
{
    {
        auto budget = std::make_shared<Budget>();
        std::vector<std::shared_ptr<Budget::ViewLease>> views;
        for (unsigned i = 0; i < Budget::kMaxViews; ++i) { auto lease = budget->ReserveView(64,64); CHECK(lease); views.push_back(lease); }
        CHECK(!budget->ReserveView(64,64));
        CHECK(!views.front()->Resize(8192,8192));
        CHECK(budget->Snapshot().surface_bytes == Budget::kMaxViews * 64 * 64 * 4);
        views.pop_back(); CHECK(budget->ReserveView(1920,1080));
        budget->SetSuspended(true); CHECK(!budget->ReserveView(64,64));
        budget->SetSuspended(false); views.clear(); CHECK(budget->Snapshot().views == 0);
    }
    {
        auto budget = std::make_shared<Budget>();
        std::array<Queue,5> queues;
        for (auto& queue : queues) CHECK(queue.SetBudget(budget));
        for (unsigned view = 0; view < 4; ++view)
            for (unsigned message = 0; message < 1024; ++message) CHECK(queues[view].Push("chunk","{}") == Queue::Result::Accepted);
        CHECK(queues[4].Push("chunk","{}") == Queue::Result::Full);
        CHECK(queues[4].Push("skyultra.visibility","{\"visible\":false}") == Queue::Result::Accepted);
        Queue replies(false); CHECK(replies.SetBudget(budget,true));
        CHECK(replies.Push("reply","{}") == Queue::Result::Accepted);
        const auto consumer = queues[0].Attach([] {}); auto flight = queues[0].Take(consumer);
        CHECK(flight && budget->Snapshot().messages[0] == 4096);
        flight->executed = true; flight.reset();
        CHECK(queues[4].Push("chunk","{}") == Queue::Result::Accepted);
        budget->SetSuspended(true);
        CHECK(queues[4].Push("chunk","{}") == Queue::Result::Suspended);
        CHECK(!queues[0].Take(consumer));
        budget->SetSuspended(false); CHECK(queues[0].Take(consumer));
        for (auto& queue : queues) queue.Close(); replies.Close();
        CHECK(budget->Snapshot().messages[0] == 0 && budget->Snapshot().bytes[0] == 0);
        CHECK(budget->Snapshot().messages[1] == 0 && budget->Snapshot().bytes[1] == 0);
    }
    {
        auto budget = std::make_shared<Budget>();
        { Queue queue; CHECK(queue.SetBudget(budget)); CHECK(queue.Push("payload","{}") == Queue::Result::Accepted); }
        CHECK(budget->Snapshot().messages[0] == 0); // Destruction balances reservations without an explicit Close.
    }
}
