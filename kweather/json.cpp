/* This file is part of KWeather, the weather applet of the KDE3 revival.
   Copyright (C) 2026 The KDE3 revival authors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "json.h"

#include <qlocale.h>
#include <qmap.h>
#include <qstring.h>
#include <qvaluelist.h>

namespace
{

class Parser
{
public:
    explicit Parser(const QByteArray &text)
        : m_text(text), m_pos(0), m_error(false), m_depth(0)
    {
    }

    QVariant parseAll()
    {
        QVariant v = value();
        skipSpace();
        if (m_pos != m_text.size())
            m_error = true;
        return v;
    }

    bool error() const { return m_error; }

private:
    char peek() const { return m_pos < m_text.size() ? m_text[m_pos] : '\0'; }

    void skipSpace()
    {
        while (m_pos < m_text.size()) {
            char c = m_text[m_pos];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                break;
            ++m_pos;
        }
    }

    bool literal(const char *word)
    {
        uint i = 0;
        for (; word[i]; ++i)
            if (m_pos + i >= m_text.size() || m_text[m_pos + i] != word[i])
                return false;
        m_pos += i;
        return true;
    }

    QVariant value()
    {
        skipSpace();
        if (m_error || ++m_depth > 64) {
            m_error = true;
            return QVariant();
        }
        QVariant v;
        char c = peek();
        if (c == '{')
            v = object();
        else if (c == '[')
            v = array();
        else if (c == '"')
            v = QVariant(string());
        else if (literal("true"))
            v = QVariant(true, 0);
        else if (literal("false"))
            v = QVariant(false, 0);
        else if (literal("null"))
            v = QVariant();
        else
            v = number();
        --m_depth;
        return v;
    }

    QVariant object()
    {
        QMap<QString, QVariant> map;
        ++m_pos;                                // {
        skipSpace();
        if (peek() == '}') {
            ++m_pos;
            return QVariant(map);
        }
        while (!m_error) {
            skipSpace();
            if (peek() != '"') {
                m_error = true;
                break;
            }
            QString key = string();
            skipSpace();
            if (peek() != ':') {
                m_error = true;
                break;
            }
            ++m_pos;
            map.insert(key, value());
            skipSpace();
            if (peek() == ',') {
                ++m_pos;
            } else if (peek() == '}') {
                ++m_pos;
                break;
            } else {
                m_error = true;
            }
        }
        return QVariant(map);
    }

    QVariant array()
    {
        QValueList<QVariant> list;
        ++m_pos;                                // [
        skipSpace();
        if (peek() == ']') {
            ++m_pos;
            return QVariant(list);
        }
        while (!m_error) {
            list.append(value());
            skipSpace();
            if (peek() == ',') {
                ++m_pos;
            } else if (peek() == ']') {
                ++m_pos;
                break;
            } else {
                m_error = true;
            }
        }
        return QVariant(list);
    }

    int hex4()
    {
        int v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = peek();
            ++m_pos;
            v <<= 4;
            if (c >= '0' && c <= '9')
                v |= c - '0';
            else if (c >= 'a' && c <= 'f')
                v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                v |= c - 'A' + 10;
            else {
                m_error = true;
                return 0;
            }
        }
        return v;
    }

    QString string()
    {
        ++m_pos;                                // "
        QCString utf8;
        QString result;
        while (!m_error) {
            if (m_pos >= m_text.size()) {
                m_error = true;
                break;
            }
            char c = m_text[m_pos++];
            if (c == '"')
                break;
            if (static_cast<unsigned char>(c) < 0x20) {   // raw control character
                m_error = true;
                break;
            }
            if (c != '\\') {
                utf8 += c;
                continue;
            }
            char e = peek();
            ++m_pos;
            switch (e) {
            case '"': utf8 += '"'; break;
            case '\\': utf8 += '\\'; break;
            case '/': utf8 += '/'; break;
            case 'b': utf8 += '\b'; break;
            case 'f': utf8 += '\f'; break;
            case 'n': utf8 += '\n'; break;
            case 'r': utf8 += '\r'; break;
            case 't': utf8 += '\t'; break;
            case 'u': {
                // flush the UTF-8 bytes, then add the UTF-16 unit(s)
                result += QString::fromUtf8(utf8);
                utf8 = QCString();
                int u = hex4();
                result += QChar(static_cast<ushort>(u));
                break;
            }
            default:
                m_error = true;
            }
        }
        result += QString::fromUtf8(utf8);
        return result;
    }

    bool digit() const { return peek() >= '0' && peek() <= '9'; }

    // -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
    QVariant number()
    {
        uint start = m_pos;
        if (peek() == '-')
            ++m_pos;
        if (peek() == '0') {
            ++m_pos;
        } else if (digit()) {
            while (digit())
                ++m_pos;
        } else {
            m_error = true;
            return QVariant();
        }
        if (peek() == '.') {
            ++m_pos;
            if (!digit()) {
                m_error = true;
                return QVariant();
            }
            while (digit())
                ++m_pos;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++m_pos;
            if (peek() == '+' || peek() == '-')
                ++m_pos;
            if (!digit()) {
                m_error = true;
                return QVariant();
            }
            while (digit())
                ++m_pos;
        }
        // not strtod(): it follows LC_NUMERIC
        QString digits = QString::fromLatin1(m_text.data() + start, m_pos - start);
        bool ok = false;
        double d = QLocale::c().toDouble(digits, &ok);
        if (!ok) {
            // out of the range of a double (1e400): not usable
            return QVariant();
        }
        return QVariant(d);
    }

    const QByteArray &m_text;
    uint m_pos;
    bool m_error;
    int m_depth;
};

}

QVariant Json::parse(const QByteArray &text, bool *ok)
{
    Parser parser(text);
    QVariant v = parser.parseAll();
    if (ok)
        *ok = !parser.error();
    return parser.error() ? QVariant() : v;
}

QVariant Json::member(const QVariant &v, const char *key)
{
    if (v.type() != QVariant::Map)
        return QVariant();
    QMap<QString, QVariant> map = v.toMap();
    QMap<QString, QVariant>::ConstIterator it = map.find(QString::fromLatin1(key));
    return it == map.end() ? QVariant() : *it;
}

double Json::number(const QVariant &v, double def)
{
    return v.type() == QVariant::Double ? v.toDouble() : def;
}
