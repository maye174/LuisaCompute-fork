//
// Created by Mike Smith on 2021/3/5.
//

#pragma once

#include <dsl/var.h>
#include <dsl/operators.h>

namespace luisa::compute {

namespace detail {

class IfStmtBuilder {

private:
    ScopeStmt *_true{nullptr};
    ScopeStmt *_false{nullptr};

public:
    explicit IfStmtBuilder(Expr<bool> condition) noexcept
        : _true{FunctionBuilder::current()->scope()},
          _false{FunctionBuilder::current()->scope()} {
        FunctionBuilder::current()->if_(condition.expression(), _true, _false);
    }

    template<typename False>
    void else_(False &&f) &&noexcept {
        FunctionBuilder::current()->with(_false, std::forward<False>(f));
    }

    template<typename True>
    auto operator%(True &&t) &&noexcept {
        FunctionBuilder::current()->with(_true, std::forward<True>(t));
        return *this;
    }

    template<typename Body>
    [[nodiscard]] auto elif (Expr<bool> condition, Body &&body) &&noexcept {
        return FunctionBuilder::current()->with(_false, [condition] { return IfStmtBuilder{condition}; }) % std::forward<Body>(body);
    }

    template<typename False>
    void operator/(False &&f) &&noexcept { IfStmtBuilder{*this}.else_(std::forward<False>(f)); }

    [[nodiscard]] auto operator/(Expr<bool> elif_cond) &&noexcept {
        return FunctionBuilder::current()->with(_false, [elif_cond] {
            return IfStmtBuilder{elif_cond};
        });
    }
};

class LoopStmtBuilder {

private:
    ScopeStmt *_body;

public:
    explicit LoopStmtBuilder() noexcept
        : _body{FunctionBuilder::current()->scope()} {
        FunctionBuilder::current()->loop_(_body);
    }

    template<typename Body>
    auto operator/(Body &&body) &&noexcept {
        FunctionBuilder::current()->with(_body, std::forward<Body>(body));
        return *this;
    }

    template<typename Body>
    void operator%(Body &&body) &&noexcept {
        LoopStmtBuilder{*this} / std::forward<Body>(body);
    }
};

class SwitchCaseStmtBuilder {

private:
    ScopeStmt *_body;

public:
    template<concepts::integral T>
    explicit SwitchCaseStmtBuilder(T c) noexcept : _body{FunctionBuilder::current()->scope()} {
        FunctionBuilder::current()->case_(extract_expression(c), _body);
    }

    template<typename Body>
    void operator%(Body &&body) &&noexcept {
        FunctionBuilder::current()->with(_body, [&body] {
            body();
            FunctionBuilder::current()->break_();
        });
    }
};

class SwitchDefaultStmtBuilder {

private:
    ScopeStmt *_body;

public:
    SwitchDefaultStmtBuilder() noexcept : _body{FunctionBuilder::current()->scope()} {
        FunctionBuilder::current()->default_(_body);
    }

    template<typename Body>
    void operator%(Body &&body) &&noexcept {
        FunctionBuilder::current()->with(_body, [&body] {
            body();
            FunctionBuilder::current()->break_();
        });
    }
};

class SwitchStmtBuilder {

private:
    ScopeStmt *_body;

public:
    template<typename T>
        requires is_integral_expr_v<T>
    explicit SwitchStmtBuilder(T &&cond) noexcept
        : _body{FunctionBuilder::current()->scope()} {
        FunctionBuilder::current()->switch_(
            extract_expression(std::forward<T>(cond)), _body);
    }

    template<typename T, typename Body>
    auto case_(T &&case_cond, Body &&case_body) &&noexcept {
        FunctionBuilder::current()->with(_body, [&case_cond, &case_body] {
            SwitchCaseStmtBuilder{case_cond} % std::forward<Body>(case_body);
        });
        return *this;
    }

    template<typename Default>
    auto default_(Default &&d) &&noexcept {
        FunctionBuilder::current()->with(_body, [&d] {
            SwitchDefaultStmtBuilder{} % std::forward<Default>(d);
        });
    }

    template<typename Body>
    void operator%(Body &&body) &&noexcept {
        FunctionBuilder::current()->with(_body, std::forward<Body>(body));
    }
};

struct ForStmtBodyInvoke {
    template<typename F>
    void operator%(F &&body) &&noexcept {
        std::invoke(std::forward<F>(body));
    }
};

template<typename T, bool has_step>
class ForRange {

    static_assert(is_integral_v<T>);

public:
    struct ForRangeEnd {};

    class ForRangeIter {
    private:
        Expr<T> _begin;
        Expr<T> _end;
        Expr<T> _step;
        const Expression *_var{nullptr};
        const Expression *_cond{nullptr};
        const ScopeStmt *_body{nullptr};
        uint _time{0u};

    public:
        explicit ForRangeIter(Expr<T> begin, Expr<T> end, Expr<T> step) noexcept
            : _begin{begin}, _end{end}, _step{step} {}

        [[nodiscard]] auto operator*() noexcept {
            if (_time != 0u) [[unlikely]] {
                LUISA_ERROR_WITH_LOCATION(
                    "Invalid RangeForIter state (with _time = {}).", _time);
            }
            auto f = FunctionBuilder::current();
            Var var{_begin};
            _var = var.expression();
            auto bool_type = Type::of<bool>();
            _cond = f->binary(bool_type, BinaryOp::LESS, _var, _end.expression());
            if constexpr (has_step) {// TODO: has step?
                // step < 0
                auto neg_step = f->binary(bool_type, BinaryOp::LESS, _step.expression(), f->literal(Type::of<T>(), T{0}));
                // ((step < 0) && !(var < end)) || (!(step < 0) && (var < end))
                _cond = f->binary(bool_type, BinaryOp::BIT_XOR, _cond, neg_step);
            }
            auto body = f->scope();
            _body = body;
            f->push_scope(body);
            return Var{std::move(var)};// to guarantee rvo
        }

        auto &operator++() noexcept {
            if (++_time == 1u) {
                auto f = FunctionBuilder::current();
                f->pop_scope(_body);
                f->for_(_var, _cond, _step.expression(), _body);
            }
            return *this;
        }

        [[nodiscard]] auto operator!=(ForRangeEnd) const noexcept {
            return _time == 0u;
        }
    };

private:
    Expr<T> _begin;
    Expr<T> _end;
    Expr<T> _step;

public:
    explicit ForRange(Expr<T> begin, Expr<T> end, Expr<T> step) noexcept
        : _begin{begin}, _end{end}, _step{step} {}
    [[nodiscard]] auto begin() const noexcept { return ForRangeIter{_begin, _end, _step}; }
    [[nodiscard]] auto end() const noexcept { return ForRangeEnd{}; }
};

// FIXME: review this...
template<typename Lhs, typename Rhs, size_t... i>
inline void assign_impl(Ref<Lhs> lhs, Expr<Rhs> rhs, std::index_sequence<i...>) noexcept {
    (dsl::assign(lhs.template get<i>(), rhs.template get<i>()), ...);
}

template<typename Lhs, typename Rhs>
inline void assign_impl(Ref<Lhs> lhs, Expr<Rhs> rhs) noexcept {
    using member_tuple = struct_member_tuple_t<expr_value_t<Lhs>>;
    assign_impl(lhs, rhs, std::make_index_sequence<std::tuple_size_v<member_tuple>>{});
}

}// namespace detail

inline namespace dsl {

template<typename Lhs, typename Rhs>
inline void assign(Lhs &&lhs, Rhs &&rhs) noexcept {
    if constexpr (concepts::assignable<expr_value_t<Lhs>, expr_value_t<Rhs>>) {
        detail::FunctionBuilder::current()->assign(
            AssignOp::ASSIGN,
            detail::extract_expression(std::forward<Lhs>(lhs)),
            detail::extract_expression(std::forward<Rhs>(rhs)));
    } else if (is_tuple_v<std::remove_cvref_t<Rhs>>) {
        assign(
            detail::Ref{std::forward<Lhs>(lhs)},
            compose(std::forward<Rhs>(rhs)));
    } else {
        detail::assign_impl(
            detail::Ref{std::forward<Lhs>(lhs)},
            Expr{std::forward<Rhs>(rhs)});
    }
}

// statements
inline void break_() noexcept { detail::FunctionBuilder::current()->break_(); }
inline void continue_() noexcept { detail::FunctionBuilder::current()->continue_(); }

template<typename True>
inline auto if_(Expr<bool> condition, True &&t) noexcept {
    return detail::IfStmtBuilder{condition} % std::forward<True>(t);
}

template<typename Body>
inline void loop(Body &&body) noexcept {
    detail::LoopStmtBuilder{} % std::forward<Body>(body);
}

template<typename T>
inline auto switch_(T &&expr) noexcept {
    return detail::SwitchStmtBuilder{std::forward<T>(expr)};
}

template<typename Te>
    requires is_integral_expr_v<Te>
[[nodiscard]] inline auto range(Te &&end) noexcept {
    using T = expr_value_t<Te>;
    Var e{std::forward<Te>(end)};
    return detail::ForRange<T, false>{static_cast<T>(0), e, static_cast<T>(1)};
}

template<typename Tb, typename Te>
    requires is_same_expr_v<Tb, Te> && is_integral_expr_v<Tb>
[[nodiscard]] inline auto range(Tb &&begin, Te &&end) noexcept {
    using T = expr_value_t<Tb>;
    Var e{std::forward<Te>(end)};
    return detail::ForRange<T, false>{std::forward<Tb>(begin), e, static_cast<T>(1)};
}

template<typename Tb, typename Te, typename Ts>
    requires is_same_expr_v<Tb, Te, Ts> && is_integral_expr_v<Tb>
[[nodiscard]] inline auto range(Tb &&begin, Te &&end, Ts &&step) noexcept {
    using T = expr_value_t<Tb>;
    Var e{std::forward<Te>(end)};
    Var s{std::forward<Ts>(step)};
    return detail::ForRange<T, true>{std::forward<Tb>(begin), e, s};
}

template<typename N, typename Body>
inline void loop(N &&n, Body &&body) noexcept {
    for (auto i : range(std::forward<N>(n))) {
        std::invoke(std::forward<Body>(body), std::move(i));
    }
}

template<typename Begin, typename End, typename Body>
inline void loop(Begin &&begin, End &&end, Body &&body) noexcept {
    for (auto i : range(std::forward<Begin>(begin), std::forward<End>(end))) {
        std::invoke(std::forward<Body>(body), std::move(i));
    }
}

template<typename Begin, typename End, typename Step, typename Body>
inline void loop(Begin &&begin, End &&end, Step &&step, Body &&body) noexcept {
    for (auto i : range(std::forward<Begin>(begin), std::forward<End>(end), std::forward<Step>(step))) {
        std::invoke(std::forward<Body>(body), std::move(i));
    }
}

template<concepts::iterable AllTags, typename Tag, typename IndexedCase, typename Otherwise>
    requires concepts::invocable<IndexedCase, int> && concepts::invocable<Otherwise>
inline void match(AllTags &&tags, Tag &&tag, IndexedCase &&indexed_case, Otherwise &&otherwise) noexcept {
    auto s = switch_(std::forward<Tag>(tag));
    auto index = 0;
    for (auto &&t : std::forward<AllTags>(tags)) {
        s = std::move(s).case_(t, [&c = indexed_case, i = index] { c(i); });
        index++;
    }
    std::move(s).default_(std::forward<Otherwise>(otherwise));
}

template<typename T, typename Tag, typename IndexedCase, typename Otherwise>
    requires concepts::invocable<IndexedCase, int> && concepts::invocable<Otherwise>
inline void match(std::initializer_list<T> all_tags, Tag &&tag, IndexedCase &&indexed_case, Otherwise &&otherwise) noexcept {
    auto s = switch_(std::forward<Tag>(tag));
    auto index = 0;
    for (auto &&t : all_tags) {
        s = std::move(s).case_(t, [&c = indexed_case, i = index] { c(i); });
        index++;
    }
    std::move(s).default_(std::forward<Otherwise>(otherwise));
}

template<typename AllTags, typename Tag, typename IndexedCase>
inline void match(AllTags &&tags, Tag &&tag, IndexedCase &&indexed_case) noexcept {
    match(std::forward<AllTags>(tags),
          std::forward<Tag>(tag),
          std::forward<IndexedCase>(indexed_case),
          [] {});
}

template<typename T, typename Tag, typename IndexedCase>
inline void match(std::initializer_list<T> tags, Tag &&tag, IndexedCase &&indexed_case) noexcept {
    match(tags, std::forward<Tag>(tag), std::forward<IndexedCase>(indexed_case), [] {});
}

inline void comment(std::string_view s) noexcept {
    detail::FunctionBuilder::current()->comment_(s);
}

}// namespace dsl
}// namespace luisa::compute
