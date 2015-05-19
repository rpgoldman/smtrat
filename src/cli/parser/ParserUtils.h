/**
 * @file ParserUtils.h
 * @author Gereon Kremer <gereon.kremer@cs.rwth-aachen.de>
 */

#pragma once

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <fstream>
#include <queue>

#include "../config.h"
#ifdef __VS
#pragma warning(push, 0)
#include <boost/spirit/include/qi.hpp>
#include <boost/variant.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#pragma warning(pop)
#else
#include <boost/spirit/include/qi.hpp>
#include <boost/variant.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#endif

#include "Common.h"
#include "VariantMap.h"
#include "ParserTypes.h"
#include "UtilityParser.h"

#include "carl/core/logging.h"

namespace smtrat {
namespace parser {

namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;
namespace px = boost::phoenix;

inline std::ostream& operator<<(std::ostream& os, const AttributeValue& value) {
	if (value.which() == 0) {
		return os << std::boolalpha << boost::get<bool>(value);
	} else {
		return boost::operator<<(os, value);
	}
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const qi::symbols<char, T>& sym) {
	os << "Symbols " << sym.name() << std::endl;
	sym.for_each([&](const std::string& key, const T& val){ os << "\t" << key << " -> " << val << std::endl; });
	return os;
}

struct TypeOfTerm : public boost::static_visitor<ExpressionType> {
	ExpressionType operator()(const FormulaT&) const { return ExpressionType::BOOLEAN; }
	ExpressionType operator()(const carl::UFInstance&) const { return ExpressionType::UNINTERPRETED; }
	ExpressionType operator()(const carl::UVariable&) const { return ExpressionType::UNINTERPRETED; }
	ExpressionType operator()(const UninterpretedType&) const { return ExpressionType::UNINTERPRETED; }
	ExpressionType operator()(const Poly&) const { return ExpressionType::THEORY; }
	ExpressionType operator()(const carl::Variable& v) const { return (*this)(v.getType()); }
	ExpressionType operator()(const carl::VariableType& v) const {
		switch (v) {
			case carl::VariableType::VT_BOOL: return ExpressionType::BOOLEAN;
			case carl::VariableType::VT_INT:
			case carl::VariableType::VT_REAL: return ExpressionType::THEORY;
			case carl::VariableType::VT_UNINTERPRETED: return ExpressionType::UNINTERPRETED;
			case carl::VariableType::VT_BITVECTOR: return ExpressionType::BITVECTOR;
			default:
				return ExpressionType::THEORY;
		}
	}
	ExpressionType operator()(const carl::Sort& v) const {
		if (carl::SortManager::getInstance().isInterpreted(v)) return (*this)(carl::SortManager::getInstance().interpretedType(v));
		else return ExpressionType::UNINTERPRETED;
	}
	template<typename T>
	static ExpressionType get(const T& t) {
		return TypeOfTerm()(t);
	}
	template<BOOST_VARIANT_ENUM_PARAMS(typename T)>
	static ExpressionType get(const boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>& var) {
		return boost::apply_visitor(TypeOfTerm(), var);
	}
};

class OutputWrapper {
	std::ostream out;
	std::string pre;
	std::string suf;
public:
	OutputWrapper(std::ostream& o, const std::string& prefix, const std::string& suffix)
	: out(o.rdbuf()), pre(prefix), suf(suffix) {
		this->out << pre;
	}
	OutputWrapper(const OutputWrapper&& o) : out(o.out.rdbuf()), pre(o.pre), suf(o.suf) {}
	~OutputWrapper() {
		this->out << suf;
	}
	template<typename T>
	OutputWrapper& operator<<(const T& t) {
		this->out << t;
		return *this;
	}
};

class InstructionHandler {
public:
	typedef smtrat::parser::AttributeValue Value;

private:
	std::queue<std::function<void()>> instructionQueue;
public:
	void addInstruction(std::function<void()> bind) {
		this->instructionQueue.push(bind);
	}
	bool hasInstructions() const {
		return !instructionQueue.empty();
	}
	void runInstructions() {
		while (!this->instructionQueue.empty()) {
			this->instructionQueue.front()();
			this->instructionQueue.pop();
		}
	}
protected:
	VariantMap<std::string, Value> infos;
	VariantMap<std::string, Value> options;
public:
	template<typename T>
	T option(const std::string& key) const {
		return this->options.get<T>(key);
	}
	bool printInstruction() const {
		if (!this->options.has<bool>("print-instruction")) return false;
		return this->options.get<bool>("print-instruction");
	}
protected:
	std::ostream mRegular;
	std::ostream mDiagnostic;
	std::map<std::string, std::ofstream> streams;

	void setStream(const std::string& s, std::ostream& os) {
		if (s == "stdout") os.rdbuf(std::cout.rdbuf());
		else if (s == "stderr") os.rdbuf(std::cerr.rdbuf());
		else if (s == "stdlog") os.rdbuf(std::clog.rdbuf());
		else {
			if (this->streams.count(s) == 0) {
				this->streams[s].open(s);
			}
			os.rdbuf(this->streams[s].rdbuf());
		}
	}
public:
	InstructionHandler(): mRegular(std::cout.rdbuf()), mDiagnostic(std::cerr.rdbuf()) {
		Attribute attr("print-instruction", AttributeMandatoryValue(false));
		this->setOption(attr);
	}
	virtual ~InstructionHandler() {
		for (auto& it: this->streams) it.second.close();
	}

	std::ostream& diagnostic() {
		return this->mDiagnostic;
	}
	void diagnostic(const std::string& s) {
		this->setStream(s, this->mDiagnostic);
	}
	std::ostream& regular() {
		return this->mRegular;
	}
	void regular(const std::string& s) {
		this->setStream(s, this->mRegular);
	}
	OutputWrapper error() {
		return OutputWrapper(mDiagnostic, "(error \"", "\")\n");
	}
	OutputWrapper warn() {
		return OutputWrapper(mDiagnostic, "(warn \"", "\")\n");
	}
	OutputWrapper info() {
		return OutputWrapper(mRegular, "(info \"", "\")\n");
	}
	virtual void add(const FormulaT& f) = 0;
	virtual void check() = 0;
	virtual void declareFun(const carl::Variable&) = 0;
	virtual void declareSort(const std::string&, const unsigned&) = 0;
	virtual void defineSort(const std::string&, const std::vector<std::string>&, const carl::Sort&) = 0;
	virtual void exit() = 0;
	virtual void getAssertions() = 0;
	virtual void getAssignment() = 0;
	void getInfo(const std::string& key) {
		if (this->infos.count(key) > 0) regular() << "(:" << key << " " << this->infos[key] << ")" << std::endl;
		else error() << "no info set for :" << key;
	}
	void getOption(const std::string& key) {
		if (this->options.count(key) > 0) regular() << "(:" << key << " " << this->options[key] << ")" << std::endl;
		else error() << "no option set for :" << key;
	}
	virtual void getProof() = 0;
	virtual void getUnsatCore() = 0;
	virtual void getValue(const std::vector<carl::Variable>&) = 0;
	virtual void pop(const unsigned&) = 0;
	virtual void push(const unsigned&) = 0;
	void setInfo(const Attribute& attr) {
		if (this->infos.count(attr.key) > 0) warn() << "overwriting info for :" << attr.key;
		if (attr.key == "name" || attr.key == "authors" || attr.key == "version") error() << "The info :" << attr.key << " is read-only.";
		else this->infos[attr.key] = attr.value;
	}
	virtual void setLogic(const smtrat::Logic&) = 0;
	void setOption(const Attribute& option)  {
		std::string key = option.key;
		if (this->options.count(key) > 0) warn() << "overwriting option for :" << key;
		this->options[key] = option.value;
		if (key == "diagnostic-output-channel") this->diagnostic(this->options.get<std::string>(key));
		else if (key == "expand-definitions") this->error() << "The option :expand-definitions is not supported.";
		else if (key == "interactive-mode") {
			this->options.assertType<bool>("interactive-mode", std::bind(&InstructionHandler::error, this));
		}
		else if (key == "print-instruction") {
			this->options.assertType<bool>("print-instruction", std::bind(&InstructionHandler::error, this));
		}
		else if (key == "print-success") {
			this->options.assertType<bool>("print-success", std::bind(&InstructionHandler::error, this));
		}
		else if (key == "produce-assignments") {
			this->options.assertType<bool>("produce-assignments", std::bind(&InstructionHandler::error, this));
		}
		else if (key == "produce-models") {
			this->options.assertType<bool>("produce-models", std::bind(&InstructionHandler::error, this));
		}
		else if (key == "produce-proofs") {
			this->error() << "The option :produce-proofs is not supported.";
		}
		else if (key == "produce-unsat-cores") {
			this->options.assertType<bool>("produce-unsat-cores", std::bind(&InstructionHandler::error, this));
		}
		else if (key == "random-seed") {
			this->error() << "The option :random-seed is not supported.";
		}
		else if (key == "regular-output-channel") this->regular(this->options.get<std::string>(key));
		else if (key == "verbosity") {
			this->options.assertType<Rational>("verbosity", std::bind(&InstructionHandler::error, this));
		}
	}
};

struct SuccessHandler {
	template<typename> struct result { typedef void type; };
	template<typename Parser, typename Rule, typename Entity, typename Begin, typename End>
	void operator()(Parser& p, const Rule& rule, const Entity& entity, Begin b, End e) const {
		p.lastrule.str("");
		p.lastrule << rule.name();
		p.lastentity.str("");
		p.lastentity << entity;
		auto line_end = std::find(b, e, '\n');
		std::cout << p.lastrule.str() << ": " << p.lastentity.str() << "\n\t" << std::string(b, line_end) << std::endl;
	}
};
struct SuccessHandlerPtr {
	template<typename> struct result { typedef void type; };
	template<typename Parser, typename Rule, typename Entity, typename Begin, typename End>
	void operator()(Parser& p, const Rule& rule, const Entity& entity, Begin b, End e) const {
		p.lastrule.str("");
		p.lastrule << rule.name();
		p.lastentity.str("");
		p.lastentity << entity;
		auto line_end = std::find(b, e, '\n');
		std::cout << p.lastrule.str() << ": " << p.lastentity.str() << "\n\t" << std::string(b, line_end) << std::endl;
	}
};

struct ErrorHandler {
	template<typename> struct result { typedef qi::error_handler_result type; };
	template<typename Parser, typename T1, typename T2, typename T3, typename T4>
	qi::error_handler_result operator()(const Parser& p, T1 b, T2 e, T3 where, T4 const& what) const {
		auto line_start = spirit::get_line_start(b, where);
		auto line_end = std::find(where, e, '\n');
		std::string line(++line_start, line_end);
		std::string input(where, line_end);
		
		std::cerr << std::endl;
		SMTRAT_LOG_ERROR("smtrat.parser", "Parsing error at " << spirit::get_line(where) << ":" << spirit::get_column(line_start, where));
		if (p.lastrule.str().size() > 0) {
			SMTRAT_LOG_ERROR("smtrat.parser", "after parsing rule " << p.lastrule.str() << ": " << p.lastentity.str());
		}
		SMTRAT_LOG_ERROR("smtrat.parser", "expected" << std::endl << "\t" << what.tag << ": " << what);
		SMTRAT_LOG_ERROR("smtrat.parser", "but got" << std::endl << "\t" << input);
		SMTRAT_LOG_ERROR("smtrat.parser", "in line " << spirit::get_line(where) << std::endl << "\t" << line);
		return qi::fail;
	}
};

}
}
