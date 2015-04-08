#pragma once

#include "Common.h"
#include "Lexicon.h"
#include "SExpression.h"
#include "theories/Theories.h"

namespace smtrat {
namespace parser {

class Attribute {
public:
	typedef types::AttributeValue AttributeValue;
	std::string key;
	AttributeValue value;

	Attribute() {}
	explicit Attribute(const std::string& key) : key(key) {}
	Attribute(const std::string& key, const AttributeValue& value) : key(key), value(value) {}
	Attribute(const std::string& key, const boost::optional<AttributeValue>& value) : key(key) {
		if (value.is_initialized()) this->value = value.get();
	}

	bool hasValue() const {
		return boost::get<boost::spirit::qi::unused_type>(&value) == nullptr;
	}
};
inline std::ostream& operator<<(std::ostream& os, const Attribute& attr) {
	os << attr.key;
	//if (attr.hasValue()) os << " " << attr.value;
	return os;
}

struct AttributeValueParser: public qi::grammar<Iterator, types::AttributeValue(), Skipper> {
	typedef VariantConverter<types::AttributeValue> Converter;
	AttributeValueParser(): AttributeValueParser::base_type(main, "attribute value") {
		main = 
				specconstant[qi::_val = px::bind(&Converter::template convert<types::ConstType>, &converter, qi::_1)]
			|	symbol[qi::_val = px::bind(&Converter::template convert<std::string>, px::ref(converter), qi::_1)]
			|	(qi::lit("(") >> *sexpression >> qi::lit(")"))[qi::_val = px::bind(&Converter::template convert<SExpressionSequence<types::ConstType>>, px::ref(converter), px::construct<SExpressionSequence<types::ConstType>>(qi::_1))]
		;
	}
	SpecConstantParser specconstant;
	SymbolParser symbol;
	SExpressionParser sexpression;
	Converter converter;
	qi::rule<Iterator, types::AttributeValue(), Skipper> main;
};


struct AttributeParser: public qi::grammar<Iterator, Attribute(), Skipper> {
	AttributeParser(): AttributeParser::base_type(main, "attribute") {
		main = (keyword > -value)[qi::_val = px::construct<Attribute>(qi::_1, qi::_2)];
	}
	KeywordParser keyword;
	AttributeValueParser value;
	qi::rule<Iterator, Attribute(), Skipper> main;
};

}
}
