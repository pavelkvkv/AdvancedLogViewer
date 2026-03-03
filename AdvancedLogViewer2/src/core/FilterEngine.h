#pragma once

#include <QRegularExpression>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

class FilterEngine {
public:
    FilterEngine() = default;
    explicit FilterEngine(const QString &expression);

    bool matches(const QString &line) const;
    bool isValid() const { return m_valid; }
    bool isEmpty() const { return m_predicates.empty(); }
    QString expression() const { return m_expression; }

    static FilterEngine compile(const QString &expression);

private:
    using Predicate = std::function<bool(const QString &)>;

    static Predicate parseToken(const QString &token);
    static Predicate makeContains(const QString &text);
    static Predicate makeWildcard(const QString &pattern);
    static Predicate makeStartsWith(const QString &arg);
    static Predicate makeEndsWith(const QString &arg);

    static QString extractFuncArg(const QString &token, const QString &funcName);

    QString m_expression;
    std::vector<Predicate> m_predicates;  // OR-список
    bool m_valid = true;
};
