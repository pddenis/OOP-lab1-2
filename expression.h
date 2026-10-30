#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

template<typename T>
using VariableMap = std::unordered_map<std::string, T>;

template<typename T>
class ExprNode {
public:
    ExprNode() {}
    ExprNode(const ExprNode&) = delete;
    ExprNode& operator=(const ExprNode&) = delete;
    virtual ~ExprNode() {}

    virtual T evaluate(const VariableMap<T>& vars) const = 0;
    virtual std::string toString() const = 0;
    virtual void collectVariables(std::set<std::string>& vars) const = 0;
    virtual int getDepth() const = 0;
    virtual int getNodeCount() const = 0;
    virtual std::unique_ptr<ExprNode<T>> simplify() const = 0;

    virtual bool isConstant() const { return false; }
    virtual T constantValue() const { throw std::logic_error("Node is not a constant"); }

    std::set<std::string> getVariables() const {
        std::set<std::string> result;
        collectVariables(result);
        return result;
    }
};

inline bool isSupportedOperator(char op) {
    return op == '+' || op == '-' || op == '*' || op == '/';
}

template<typename T>
T applyOperator(char op, const T& l, const T& r) {
    switch (op) {
    case '+': return l + r;
    case '-': return l - r;
    case '*': return l * r;
    case '/':
        if (r == T(0)) throw std::runtime_error("Division by zero");
        return l / r;
    default:
        throw std::invalid_argument(std::string("Unknown operator: ") + op);
    }
}

template<typename T>
class ConstantNode : public ExprNode<T> {
private:
    T value_;

public:
    explicit ConstantNode(const T& value) : value_(value) {}

    T evaluate(const VariableMap<T>&) const override { return value_; }
    std::string toString() const override {
        std::ostringstream os;
        os << value_;
        return os.str();
    }
    void collectVariables(std::set<std::string>&) const override {}
    int getDepth() const override { return 0; }
    int getNodeCount() const override { return 1; }
    std::unique_ptr<ExprNode<T>> simplify() const override {
        return std::unique_ptr<ExprNode<T>>(new ConstantNode<T>(value_));
    }
    bool isConstant() const override { return true; }
    T constantValue() const override { return value_; }
};

template<typename T>
class VariableNode : public ExprNode<T> {
private:
    std::string name_;

public:
    explicit VariableNode(const std::string& name) : name_(name) {
        if (name_.empty()) throw std::invalid_argument("Variable name must not be empty");
    }

    T evaluate(const VariableMap<T>& vars) const override {
        typename VariableMap<T>::const_iterator it = vars.find(name_);
        if (it == vars.end()) throw std::runtime_error("Unknown variable: " + name_);
        return it->second;
    }
    std::string toString() const override { return name_; }
    void collectVariables(std::set<std::string>& vars) const override { vars.insert(name_); }
    int getDepth() const override { return 0; }
    int getNodeCount() const override { return 1; }
    std::unique_ptr<ExprNode<T>> simplify() const override {
        return std::unique_ptr<ExprNode<T>>(new VariableNode<T>(name_));
    }
};

template<typename T>
class BinaryOpNode : public ExprNode<T> {
private:
    char op_;
    std::unique_ptr<ExprNode<T>> left_;
    std::unique_ptr<ExprNode<T>> right_;

public:
    BinaryOpNode(char op, std::unique_ptr<ExprNode<T>> left, std::unique_ptr<ExprNode<T>> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {
        if (!left_ || !right_) throw std::invalid_argument("Binary operation needs exactly two operands");
        if (!isSupportedOperator(op_)) throw std::invalid_argument(std::string("Unknown operator: ") + op_);
    }

    T evaluate(const VariableMap<T>& vars) const override {
        const T l = left_->evaluate(vars);
        const T r = right_->evaluate(vars);
        return applyOperator(op_, l, r);
    }

    std::string toString() const override {
        return "(" + left_->toString() + " " + op_ + " " + right_->toString() + ")";
    }

    void collectVariables(std::set<std::string>& vars) const override {
        left_->collectVariables(vars);
        right_->collectVariables(vars);
    }

    int getDepth() const override { return 1 + std::max(left_->getDepth(), right_->getDepth()); }
    int getNodeCount() const override { return 1 + left_->getNodeCount() + right_->getNodeCount(); }

    std::unique_ptr<ExprNode<T>> simplify() const override {
        std::unique_ptr<ExprNode<T>> l = left_->simplify();
        std::unique_ptr<ExprNode<T>> r = right_->simplify();
        if (l->isConstant() && r->isConstant()) {
            try {
                const T value = applyOperator(op_, l->constantValue(), r->constantValue());
                return std::unique_ptr<ExprNode<T>>(new ConstantNode<T>(value));
            }
            catch (const std::runtime_error&) {}
        }
        return std::unique_ptr<ExprNode<T>>(new BinaryOpNode<T>(op_, std::move(l), std::move(r)));
    }
};

template<typename T>
std::unique_ptr<ExprNode<T>> makeConst(const T& value) {
    return std::unique_ptr<ExprNode<T>>(new ConstantNode<T>(value));
}

template<typename T>
std::unique_ptr<ExprNode<T>> makeVar(const std::string& name) {
    return std::unique_ptr<ExprNode<T>>(new VariableNode<T>(name));
}

template<typename T>
std::unique_ptr<ExprNode<T>> makeBin(char op, std::unique_ptr<ExprNode<T>> l, std::unique_ptr<ExprNode<T>> r) {
    return std::unique_ptr<ExprNode<T>>(new BinaryOpNode<T>(op, std::move(l), std::move(r)));
}

#endif // EXPRESSION_H