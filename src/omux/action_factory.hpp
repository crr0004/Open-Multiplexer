#pragma once
#include "actions.hpp"
#include <memory>
#include <vector>
#include <string>
namespace omux {
    class ActionFactory {

        std::vector<std::unique_ptr<Action>> action_stack;

        public:
        ActionFactory() = default;
        auto get_action_stack() -> std::vector<Action::ptr>*;
        Actions process_to_action(std::string&);
    };
}