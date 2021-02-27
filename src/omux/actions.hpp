#pragma once

#include <memory>
namespace omux {
    enum Actions { prefix, none, split_vert };
    constexpr auto PREFIX_CODE = '\x1';
    constexpr auto SPLIT_VERT_CODE = '\x23';
    class PrimaryConsole;
    class Action {

        public:
        using ptr = std::unique_ptr<Action>;
        Action() = default;
        virtual ~Action() = default;
        virtual auto get_enum() -> omux::Actions;
        virtual auto act(PrimaryConsole*) -> bool;
        virtual auto undo() -> bool;
    };
    class SplitVertAction : public Action {
        public:
        SplitVertAction();
        virtual ~SplitVertAction();
        virtual auto get_enum() -> omux::Actions;
        virtual auto act(PrimaryConsole*) -> bool;
        virtual auto undo() -> bool;
    };
    class PrefixAction : public Action {
        public:
        PrefixAction();
        virtual ~PrefixAction();
        virtual auto get_enum() -> omux::Actions;
        virtual auto act(PrimaryConsole*) -> bool;
        virtual auto undo() -> bool;
    };
    
} // namespace omux