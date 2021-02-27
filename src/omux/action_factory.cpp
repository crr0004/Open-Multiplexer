#include "omux/action_factory.hpp"

auto omux::ActionFactory::get_action_stack() -> std::vector<Action::ptr>* {
    return &action_stack;
}
auto omux::ActionFactory::process_to_action(std::string& input) -> Actions {
    Actions action_store = Actions::none;
    auto start = input.begin();
    while(start != input.end()) {
        auto character = *start;
        switch(character) {
            
            case PREFIX_CODE: {
                
                action_stack.push_back(std::make_unique<PrefixAction>());
                action_store = Actions::prefix;
                start = input.erase(start);
                break;
            }
            case SPLIT_VERT_CODE: {
                
                if(!action_stack.empty() && action_stack.back()->get_enum() == Actions::prefix) {
                    action_stack.push_back(std::make_unique<SplitVertAction>());
                    action_store = Actions::prefix;
                }       
                start = input.erase(start);
                break;
            }
            default: {
                // A character has been hit that isn't part of the keys so we need to remove
                // the prefix
                if(!action_stack.empty() && action_stack.back()->get_enum() == Actions::prefix) {
                    action_stack.pop_back();
                }
                start++;
                break;
            }
        }


    }
    return action_store;
}