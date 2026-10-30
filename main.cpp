#include "expression.h"

#include <cmath>
#include <iostream>
#include <string>

typedef std::unique_ptr<ExprNode<double>> Expr;

// ---------- мини-фреймворк тестов ----------

static int g_passed = 0;
static int g_failed = 0;

static void check(bool condition, const std::string& name) {
    if (condition) { ++g_passed; std::cout << "[PASS] " << name << "\n"; }
    else { ++g_failed; std::cerr << "[FAIL] " << name << "\n"; }
}

// Проверяет тип исключения и фрагмент сообщения.
template<typename Ex, typename F>
static void checkThrows(F f, const std::string& fragment, const std::string& name) {
    bool ok = false;
    try { f(); }
    catch (const Ex& e) { ok = std::string(e.what()).find(fragment) != std::string::npos; }
    catch (...) {}
    check(ok, name);
}

static bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

static Expr twoPlusThree() { return makeBin<double>('+', makeConst(2.0), makeConst(3.0)); }

static Expr sumTimesPrice() { return makeBin<double>('*', twoPlusThree(), makeVar<double>("price")); }

// ---------- тесты ----------

static void testBasics() {
    Expr e = sumTimesPrice();
    VariableMap<double> vars;
    vars["price"] = 10.0;

    check(e->toString() == "((2 + 3) * price)", "toString");
    check(e->getNodeCount() == 5, "node count");
    check(e->getDepth() == 2, "depth");
    check(makeConst(1.0)->getDepth() == 0 && makeConst(1.0)->getNodeCount() == 1, "leaf depth and count");
    check(e->getVariables() == std::set<std::string>{"price"}, "variables");
    check(near(e->evaluate(vars), 50.0), "evaluate");
    vars["price"] = 4.0;
    check(near(e->evaluate(vars), 20.0), "evaluate with other values");

    Expr multi = makeBin<double>('+',
        makeBin<double>('*', makeVar<double>("price"), makeVar<double>("count")),
        makeVar<double>("delivery"));
    VariableMap<double> v3;
    v3["price"] = 10.0; v3["count"] = 3.0; v3["delivery"] = 5.0;
    check(multi->toString() == "((price * count) + delivery)", "multi-variable toString");
    check(near(multi->evaluate(v3), 35.0), "multi-variable evaluate");
    check(multi->getVariables() == std::set<std::string>({ "count", "delivery", "price" }), "multi-variable set");
    check(multi->getNodeCount() == 5 && multi->getDepth() == 2, "multi-variable count and depth");

    Expr minus = makeBin<double>('-', makeConst(7.0), makeConst(2.5));
    Expr divide = makeBin<double>('/', makeConst(7.0), makeConst(2.0));
    check(near(minus->evaluate(VariableMap<double>()), 4.5), "subtraction");
    check(near(divide->evaluate(VariableMap<double>()), 3.5), "division");
    check(makeConst(5.0)->getVariables().empty(), "constant has no variables");
}

static void testSimplify() {
    Expr e = sumTimesPrice();
    Expr s = e->simplify();
    check(s->toString() == "(5 * price)" && s->getNodeCount() == 3, "simplify folds constant subtree");
    check(e->toString() == "((2 + 3) * price)" && e->getNodeCount() == 5, "original unchanged after simplify");
    check(s.get() != e.get(), "simplify returns new tree");

    VariableMap<double> vars;
    vars["price"] = 4.0;
    check(near(s->evaluate(vars), e->evaluate(vars)), "simplified tree evaluates the same");

    Expr constOnly = makeBin<double>('*', twoPlusThree(), makeConst(4.0));
    Expr folded = constOnly->simplify();
    check(folded->isConstant() && near(folded->constantValue(), 20.0) &&
        folded->toString() == "20" && folded->getNodeCount() == 1, "constants-only folds to one node");
    check(constOnly->getNodeCount() == 5, "constants-only original unchanged");

    Expr nested = makeBin<double>('+',
        makeBin<double>('*', twoPlusThree(), makeVar<double>("x")),
        makeBin<double>('-', makeConst(10.0), makeConst(4.0)));
    check(nested->simplify()->toString() == "((5 * x) + 6)", "simplify at several levels");

    Expr noFold = makeBin<double>('+', makeVar<double>("a"), makeVar<double>("b"));
    check(noFold->simplify()->toString() == "(a + b)", "nothing to fold");

    checkThrows<std::logic_error>([] { makeVar<double>("x")->constantValue(); }, "not a constant",
        "constantValue of non-constant throws");
}

static void testErrors() {
    Expr e = sumTimesPrice();
    checkThrows<std::runtime_error>([&] { e->evaluate(VariableMap<double>()); }, "Unknown variable: price",
        "unknown variable");

    Expr divZero = makeBin<double>('/', makeConst(10.0),
        makeBin<double>('-', makeConst(5.0), makeConst(5.0)));
    checkThrows<std::runtime_error>([&] { divZero->evaluate(VariableMap<double>()); }, "Division by zero",
        "division by zero");
    Expr divZeroS = divZero->simplify();
    check(divZeroS->toString() == "(10 / 0)" && divZeroS->getNodeCount() == 3, "simplify keeps division by zero");
    checkThrows<std::runtime_error>([&] { divZeroS->evaluate(VariableMap<double>()); }, "Division by zero",
        "kept node still fails on evaluate");

    Expr tiny = makeBin<double>('/', makeConst(1.0), makeConst(1e-12));
    check(std::abs(tiny->evaluate(VariableMap<double>()) - 1e12) < 1.0, "tiny divisor is not zero");

    checkThrows<std::invalid_argument>([] { makeBin<double>('+', Expr(), makeConst(1.0)); }, "two operands",
        "null left operand rejected");
    checkThrows<std::invalid_argument>([] { makeBin<double>('+', makeConst(1.0), Expr()); }, "two operands",
        "null right operand rejected");
    checkThrows<std::invalid_argument>([] { makeBin<double>('^', makeConst(1.0), makeConst(2.0)); },
        "Unknown operator", "unknown operator rejected");
    checkThrows<std::invalid_argument>([] { makeVar<double>(""); }, "must not be empty", "empty variable name rejected");
}

static void testOtherNumericType() {
    std::unique_ptr<ExprNode<int>> div = makeBin<int>('/', makeConst(7), makeConst(2));
    check(div->evaluate(VariableMap<int>()) == 3, "int: integer division");
    std::unique_ptr<ExprNode<int>> zero = makeBin<int>('/', makeConst(7), makeConst(0));
    checkThrows<std::runtime_error>([&] { zero->evaluate(VariableMap<int>()); }, "Division by zero",
        "int: division by zero");
    check(zero->simplify()->toString() == "(7 / 0)", "int: simplify keeps division by zero");
    check(makeBin<int>('+', makeConst(1), makeConst(2))->simplify()->constantValue() == 3, "int: folding");
}

// ---------- демонстрация ----------

static void printVariables(const std::set<std::string>& vars) {
    std::cout << "{";
    bool first = true;
    for (std::set<std::string>::const_iterator it = vars.begin(); it != vars.end(); ++it) {
        std::cout << (first ? "" : ", ") << *it;
        first = false;
    }
    std::cout << "}";
}

static void demo() {
    std::cout << "=== DEMO ===\n";
    Expr onlyConst = makeBin<double>('*', twoPlusThree(), makeConst(4.0));
    Expr oneVar = sumTimesPrice();
    Expr manyVars = makeBin<double>('+',
        makeBin<double>('*', makeVar<double>("price"), makeVar<double>("count")), makeVar<double>("delivery"));

    std::cout << "constants only : " << onlyConst->toString() << " = " << onlyConst->evaluate(VariableMap<double>()) << "\n";
    std::cout << "one variable   : " << oneVar->toString() << "\n";
    std::cout << "many variables : " << manyVars->toString() << "\n";

    for (int price = 10; price <= 30; price += 10) {
        VariableMap<double> vars;
        vars["price"] = price;
        std::cout << "  price=" << price << " -> " << oneVar->evaluate(vars) << "\n";
    }

    std::cout << "variables of many: ";
    printVariables(manyVars->getVariables());
    std::cout << "\ndepth/nodes of many: " << manyVars->getDepth() << "/" << manyVars->getNodeCount() << "\n";

    Expr simplified = oneVar->simplify();
    std::cout << "simplify: " << oneVar->toString() << " -> " << simplified->toString()
        << " (original still " << oneVar->toString() << ")\n";

    try { oneVar->evaluate(VariableMap<double>()); }
    catch (const std::exception& e) { std::cout << "error: " << e.what() << "\n"; }
    try { makeBin<double>('/', makeConst(1.0), makeConst(0.0))->evaluate(VariableMap<double>()); }
    catch (const std::exception& e) { std::cout << "error: " << e.what() << "\n"; }
}

int main() {
    std::cout << "=== TESTS ===\n";
    testBasics();
    testSimplify();
    testErrors();
    testOtherNumericType();
    std::cout << "=== passed: " << g_passed << ", failed: " << g_failed << " ===\n\n";
    demo();
    return g_failed == 0 ? 0 : 1;
}