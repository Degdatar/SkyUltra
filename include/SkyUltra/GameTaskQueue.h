#pragma once

#include <SKSE/Interfaces.h>

#include <atomic>
#include <functional>
#include <utility>

namespace skyultra {

class GameTaskQueue final {
public:
    void Bind(const SKSE::TaskInterface* task_interface) noexcept
    {
        task_interface_.store(task_interface, std::memory_order_release);
    }

    [[nodiscard]] bool Post(std::function<void()> task) const
    {
        const auto* task_interface = task_interface_.load(std::memory_order_acquire);
        if (task_interface == nullptr || !task) {
            return false;
        }
        task_interface->AddTask(std::move(task));
        return true;
    }

private:
    std::atomic<const SKSE::TaskInterface*> task_interface_{};
};

}  // namespace skyultra
