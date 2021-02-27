#include "omux/actions.hpp"
#include "console.hpp"
using namespace omux;

auto Action::get_enum() -> omux::Actions {
    return Actions::none;
}
auto Action::act(PrimaryConsole* console) -> bool {
    return false;
}
auto Action::undo() -> bool {
    return false;
}
SplitVertAction::SplitVertAction() {

}
SplitVertAction::~SplitVertAction() {
}
auto SplitVertAction::act(PrimaryConsole* console) -> bool {
    console->split_active_console(SPLIT_DIRECTION::VERT);
    return true;
}
auto SplitVertAction::get_enum() -> Actions {
    return Actions::split_vert;
}
auto SplitVertAction::undo() -> bool {
    return false;
}

PrefixAction::PrefixAction() {
}
PrefixAction::~PrefixAction() {
}
auto PrefixAction::act(PrimaryConsole* console) -> bool {
    return false;
}
auto PrefixAction::get_enum() -> Actions {
    return Actions::prefix;
}
auto PrefixAction::undo() -> bool {
    return false;
}