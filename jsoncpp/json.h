#pragma once

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Json
{
enum ValueType
{
	nullValue = 0,
	intValue,
	arrayValue,
	objectValue
};

class Value
{
public:
	Value()
		: type_(nullValue), intValue_(0)
	{
	}

	explicit Value(ValueType type)
		: type_(type), intValue_(0)
	{
	}

	explicit Value(int value)
		: type_(intValue), intValue_(value)
	{
	}

	bool isNull() const
	{
		return type_ == nullValue;
	}

	int asInt() const
	{
		return intValue_;
	}

	unsigned size() const
	{
		if (type_ == arrayValue)
			return static_cast<unsigned>(array_.size());
		if (type_ == objectValue)
			return static_cast<unsigned>(object_.size());
		return 0;
	}

	void append(int value)
	{
		ensureArray();
		array_.push_back(Value(value));
	}

	void append(const Value &value)
	{
		ensureArray();
		array_.push_back(value);
	}

	Value &operator[](unsigned index)
	{
		ensureArray();
		if (index >= array_.size())
			array_.resize(index + 1);
		return array_[index];
	}

	const Value &operator[](unsigned index) const
	{
		static const Value nullValueInstance;
		if (type_ != arrayValue || index >= array_.size())
			return nullValueInstance;
		return array_[index];
	}

	Value &operator[](const std::string &key)
	{
		ensureObject();
		return object_[key];
	}

	const Value &operator[](const std::string &key) const
	{
		static const Value nullValueInstance;
		if (type_ != objectValue)
			return nullValueInstance;
		auto it = object_.find(key);
		if (it == object_.end())
			return nullValueInstance;
		return it->second;
	}

	Value &operator=(int value)
	{
		type_ = intValue;
		intValue_ = value;
		array_.clear();
		object_.clear();
		return *this;
	}

	Value &operator=(const Value &other) = default;

	ValueType type() const
	{
		return type_;
	}

	const std::vector<Value> &arrayItems() const
	{
		return array_;
	}

	const std::map<std::string, Value> &objectItems() const
	{
		return object_;
	}

private:
	void ensureArray()
	{
		if (type_ == arrayValue)
			return;
		type_ = arrayValue;
		intValue_ = 0;
		array_.clear();
		object_.clear();
	}

	void ensureObject()
	{
		if (type_ == objectValue)
			return;
		type_ = objectValue;
		intValue_ = 0;
		array_.clear();
		object_.clear();
	}

	ValueType type_;
	int intValue_;
	std::vector<Value> array_;
	std::map<std::string, Value> object_;
};

class Reader
{
public:
	bool parse(const std::string &document, Value &root)
	{
		text_ = &document;
		index_ = 0;
		skipWhitespace();
		if (!parseValue(root))
			return false;
		skipWhitespace();
		return index_ == text_->size();
	}

private:
	bool parseValue(Value &out)
	{
		skipWhitespace();
		if (index_ >= text_->size())
			return false;

		char current = (*text_)[index_];
		if (current == '{')
			return parseObject(out);
		if (current == '[')
			return parseArray(out);
		if (current == '"')
		{
			std::string ignored;
			return parseString(ignored);
		}
		if (current == '-' || std::isdigit(static_cast<unsigned char>(current)))
			return parseInt(out);
		if (matchLiteral("null"))
		{
			out = Value();
			return true;
		}
		return false;
	}

	bool parseObject(Value &out)
	{
		out = Value(objectValue);
		++index_;
		skipWhitespace();
		if (consume('}'))
			return true;

		while (index_ < text_->size())
		{
			std::string key;
			if (!parseString(key))
				return false;
			skipWhitespace();
			if (!consume(':'))
				return false;
			Value child;
			if (!parseValue(child))
				return false;
			out[key] = child;
			skipWhitespace();
			if (consume('}'))
				return true;
			if (!consume(','))
				return false;
			skipWhitespace();
		}
		return false;
	}

	bool parseArray(Value &out)
	{
		out = Value(arrayValue);
		++index_;
		skipWhitespace();
		if (consume(']'))
			return true;

		while (index_ < text_->size())
		{
			Value item;
			if (!parseValue(item))
				return false;
			out.append(item);
			skipWhitespace();
			if (consume(']'))
				return true;
			if (!consume(','))
				return false;
			skipWhitespace();
		}
		return false;
	}

	bool parseString(std::string &out)
	{
		if (!consume('"'))
			return false;
		std::ostringstream builder;
		while (index_ < text_->size())
		{
			char current = (*text_)[index_++];
			if (current == '"')
			{
				out = builder.str();
				return true;
			}
			if (current == '\\')
			{
				if (index_ >= text_->size())
					return false;
				char escaped = (*text_)[index_++];
				switch (escaped)
				{
				case '"':
				case '\\':
				case '/':
					builder << escaped;
					break;
				case 'b':
					builder << '\b';
					break;
				case 'f':
					builder << '\f';
					break;
				case 'n':
					builder << '\n';
					break;
				case 'r':
					builder << '\r';
					break;
				case 't':
					builder << '\t';
					break;
				default:
					return false;
				}
			}
			else
			{
				builder << current;
			}
		}
		return false;
	}

	bool parseInt(Value &out)
	{
		std::size_t start = index_;
		if ((*text_)[index_] == '-')
			++index_;
		while (index_ < text_->size() && std::isdigit(static_cast<unsigned char>((*text_)[index_])))
			++index_;
		if (start == index_)
			return false;
		int number = std::atoi(text_->substr(start, index_ - start).c_str());
		out = Value(number);
		return true;
	}

	bool matchLiteral(const char *literal)
	{
		std::size_t literalIndex = 0;
		std::size_t cursor = index_;
		while (literal[literalIndex] != '\0')
		{
			if (cursor >= text_->size() || (*text_)[cursor] != literal[literalIndex])
				return false;
			++cursor;
			++literalIndex;
		}
		index_ = cursor;
		return true;
	}

	bool consume(char expected)
	{
		if (index_ < text_->size() && (*text_)[index_] == expected)
		{
			++index_;
			return true;
		}
		return false;
	}

	void skipWhitespace()
	{
		while (index_ < text_->size() && std::isspace(static_cast<unsigned char>((*text_)[index_])))
			++index_;
	}

	const std::string *text_ = nullptr;
	std::size_t index_ = 0;
};

class FastWriter
{
public:
	std::string write(const Value &value)
	{
		return serialize(value) + "\n";
	}

private:
	std::string serialize(const Value &value)
	{
		switch (value.type())
		{
		case nullValue:
			return "null";
		case intValue:
			return std::to_string(value.asInt());
		case arrayValue:
			return serializeArray(value);
		case objectValue:
			return serializeObject(value);
		default:
			return "null";
		}
	}

	std::string serializeArray(const Value &value)
	{
		std::ostringstream out;
		out << '[';
		const auto &items = value.arrayItems();
		for (std::size_t i = 0; i < items.size(); ++i)
		{
			if (i != 0)
				out << ',';
			out << serialize(items[i]);
		}
		out << ']';
		return out.str();
	}

	std::string serializeObject(const Value &value)
	{
		std::ostringstream out;
		out << '{';
		bool first = true;
		for (const auto &entry : value.objectItems())
		{
			if (!first)
				out << ',';
			first = false;
			out << '"' << escape(entry.first) << '"' << ':' << serialize(entry.second);
		}
		out << '}';
		return out.str();
	}

	std::string escape(const std::string &text)
	{
		std::ostringstream out;
		for (char current : text)
		{
			switch (current)
			{
			case '\\':
				out << "\\\\";
				break;
			case '"':
				out << "\\\"";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				out << current;
				break;
			}
		}
		return out.str();
	}
};
} // namespace Json