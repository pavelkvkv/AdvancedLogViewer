#include "FilterEngine.h"

#include <QRegularExpression>

FilterEngine::FilterEngine(const QString &expression)
    : m_expression(expression)
{
    if (expression.trimmed().isEmpty()) {
        m_valid = true;
        return;
    }

    const QStringList parts = expression.split(QLatin1Char('|'));
    for (const QString &part : parts) {
        QString token = part.trimmed();
        if (token.isEmpty()) {
            continue;
        }
        auto pred = parseToken(token);
        if (!pred) {
            m_valid = false;
            return;
        }
        m_predicates.push_back(std::move(pred));
    }
}

bool FilterEngine::matches(const QString &line) const
{
    if (m_predicates.empty()) {
        return true;
    }
    for (const auto &pred : m_predicates) {
        if (pred(line)) {
            return true;
        }
    }
    return false;
}

FilterEngine FilterEngine::compile(const QString &expression)
{
    return FilterEngine(expression);
}

FilterEngine::Predicate FilterEngine::parseToken(const QString &token)
{
    // startswith("...")
    QString arg = extractFuncArg(token, QStringLiteral("startswith"));
    if (!arg.isNull()) {
        return makeStartsWith(arg);
    }

    // endswith("...")
    arg = extractFuncArg(token, QStringLiteral("endswith"));
    if (!arg.isNull()) {
        return makeEndsWith(arg);
    }

    // contains("...")
    arg = extractFuncArg(token, QStringLiteral("contains"));
    if (!arg.isNull()) {
        return makeContains(arg);
    }

    // Wildcard: содержит * или ?
    if (token.contains(QLatin1Char('*')) || token.contains(QLatin1Char('?'))) {
        return makeWildcard(token);
    }

    // По умолчанию — contains (подстрока)
    return makeContains(token);
}

FilterEngine::Predicate FilterEngine::makeContains(const QString &text)
{
    QString lower = text.toLower();
    return [lower](const QString &line) {
        return line.toLower().contains(lower);
    };
}

FilterEngine::Predicate FilterEngine::makeWildcard(const QString &pattern)
{
    // Ручная конвертация wildcard → regex: экранируем все метасимволы,
    // кроме * и ?, которые преобразуем в .* и .
    QString regexStr;
    regexStr.reserve(pattern.size() * 2 + 4);
    regexStr += QLatin1String("\\A");

    for (const QChar ch : pattern) {
        if (ch == QLatin1Char('*')) {
            regexStr += QLatin1String(".*");
        } else if (ch == QLatin1Char('?')) {
            regexStr += QLatin1Char('.');
        } else if (QLatin1String("\\^$.|+()[]{}").contains(ch)) {
            regexStr += QLatin1Char('\\');
            regexStr += ch;
        } else {
            regexStr += ch;
        }
    }
    regexStr += QLatin1String("\\z");

    QRegularExpression re(regexStr,
                          QRegularExpression::CaseInsensitiveOption |
                          QRegularExpression::DotMatchesEverythingOption);

    if (!re.isValid()) {
        return nullptr;
    }

    auto shared = std::make_shared<QRegularExpression>(std::move(re));
    return [shared](const QString &line) {
        return shared->match(line).hasMatch();
    };
}

FilterEngine::Predicate FilterEngine::makeStartsWith(const QString &arg)
{
    QString lower = arg.toLower();
    return [lower](const QString &line) {
        return line.toLower().startsWith(lower);
    };
}

FilterEngine::Predicate FilterEngine::makeEndsWith(const QString &arg)
{
    QString lower = arg.toLower();
    return [lower](const QString &line) {
        return line.toLower().endsWith(lower);
    };
}

QString FilterEngine::extractFuncArg(const QString &token, const QString &funcName)
{
    QString trimmed = token.trimmed();
    if (!trimmed.toLower().startsWith(funcName + QLatin1Char('('))) {
        return {};
    }
    if (!trimmed.endsWith(QLatin1Char(')'))) {
        return {};
    }

    int start = funcName.length() + 1;
    int end = trimmed.length() - 1;
    QString inner = trimmed.mid(start, end - start).trimmed();

    // Убрать кавычки, если есть
    if (inner.length() >= 2 &&
        ((inner.startsWith(QLatin1Char('"')) && inner.endsWith(QLatin1Char('"'))) ||
         (inner.startsWith(QLatin1Char('\'')) && inner.endsWith(QLatin1Char('\''))))) {
        inner = inner.mid(1, inner.length() - 2);
    }

    return inner;
}
