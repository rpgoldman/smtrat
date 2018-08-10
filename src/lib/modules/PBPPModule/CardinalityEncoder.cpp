#include"CardinalityEncoder.h"

#include <array>
#include <deque>
#include <iterator>

namespace smtrat {
	boost::optional<FormulaT> CardinalityEncoder::doEncode(const ConstraintT& constraint) {
		bool allCoeffPositive = true;
		bool allCoeffNegative = true;
		unsigned numberOfTerms = 0;

		for (const auto& it : constraint.lhs()) {
			if (it.isConstant()) continue;
			assert(it.coeff() == 1 || it.coeff() == -1);

			if (it.coeff() < 0) allCoeffPositive = false;
			if (it.coeff() > 0) allCoeffNegative = false;

			numberOfTerms += 1;
		}

		assert(!allCoeffNegative || !allCoeffPositive);

		bool mixedCoeff = !allCoeffNegative && !allCoeffPositive;
		Rational constant = -constraint.constantPart();

		if (constraint.relation() == carl::Relation::EQ && !mixedCoeff) {
			// For equality -x1 - x2 - x3 ~ -2 and x1 + x2 + x3 ~ 2 are the same
			ConstraintT normalizedConstraint;
			if (allCoeffNegative) {
				normalizedConstraint = ConstraintT(constraint.lhs() * Rational(-1), constraint.relation());
				constant = constant * Rational(-1);
			} else {
				normalizedConstraint = constraint;
			}

			// x1 + x2 + x3 = -1 or -x1 - x2 - x3 = 1
			if (constant < 0) return FormulaT(carl::FormulaType::FALSE);
			// x1 + x2 + x3 = 4 or -x1 - x2 - x3 = -4
			if (numberOfTerms < constant) return FormulaT(carl::FormulaType::FALSE);

			return encodeExactly(normalizedConstraint);
		} else if (!mixedCoeff) {
			// we only expect normalized constraints!
			assert(constraint.relation() == carl::Relation::LEQ);

			// -x1 -x2 -x3 <= -4 iff x1 + x2 + x3 >= 4
			if (allCoeffNegative && carl::abs(constant) > constraint.variables().size())
				return FormulaT(carl::FormulaType::FALSE);
			// -x1 - x2 - x3 <= 1 iff. x1 + x2 + x3 >= -1
			if (allCoeffNegative && constant > 0) return FormulaT(carl::FormulaType::FALSE);
			// x1 + x2 + x3 <= 10
			if (allCoeffPositive && constant >= constraint.variables().size())
				return FormulaT(carl::FormulaType::TRUE);

			if (allCoeffNegative) return encodeAtLeast(constraint);
			else if (allCoeffPositive) return encodeAtMost(constraint);
			else assert(false);

		} else {
			assert(mixedCoeff);

			// TODO we probably want to put more thought into this
			return {};
		}

		assert(false && "All cases should have been handled - a return statement is missing");
		return {};
	}

	boost::optional<FormulaT> CardinalityEncoder::encodeExactly(const ConstraintT& constraint) {
		// if (!encodeAsBooleanFormula(constraint)) return boost::none;

		return encodeExactly(constraint.variables(), -constraint.constantPart());
	}

	FormulaT CardinalityEncoder::encodeExactly(const std::set<carl::Variable>& variables, const Rational constant) {
		// assert(variables.size() > constant && "This should have been handled before!");
		assert(constant >= 0 && "This should have been handled before!");

		// either a permutation contains the negation of the variable or the positive variable
		// This has nothing to do with the actual coefficient signs!
		std::deque<bool> signs;

		for (unsigned int i = 0; i < variables.size(); i++) {
			if (i < constant) signs.push_front(true);
			else signs.push_front(false);
		}

		// TODO copying is probably not what we want but we can not easily access the elements
		std::vector<carl::Variable> variableVector(variables.begin(), variables.end());

		FormulasT resultFormulaSet;
		do {
			FormulasT terms;
			for (unsigned i = 0; i < signs.size(); i++) {
				if(signs[i]){
					terms.push_back(FormulaT(variableVector[i]));
				}else{
					terms.push_back(FormulaT(carl::FormulaType::NOT, FormulaT(variableVector[i])));
				}
			}

			resultFormulaSet.push_back(FormulaT(carl::FormulaType::AND, std::move(terms)));
		} while(std::next_permutation(std::begin(signs), std::end(signs)));

		FormulaT resultFormula = FormulaT(carl::FormulaType::OR, resultFormulaSet);
		SMTRAT_LOG_DEBUG("smtrat.pbc", "Encoding exactly: " <<
				variables << " = " << constant
				<< " as " << resultFormula);

		return resultFormula;
	}

	boost::optional<FormulaT> CardinalityEncoder::encodeAtLeast(const ConstraintT& constraint) {
		// if (!encodeAsBooleanFormula(constraint)) return boost::none;

		FormulasT allOrSet;
		for (const auto& variable: constraint.variables()) {
			allOrSet.push_back(FormulaT(variable));
		}

		FormulasT result;
		Rational constant = constraint.constantPart();
		assert(constant > 0);
		for (Rational i = constant - 1; i > 0; i--) {
			result.push_back(!encodeExactly(constraint.variables(), i));
		}

		return FormulaT(carl::FormulaType::AND,
				FormulaT(carl::FormulaType::AND, result),
				FormulaT(carl::FormulaType::OR, allOrSet));
	}

	boost::optional<FormulaT> CardinalityEncoder::encodeAtMost(const ConstraintT& constraint) {
		// if (!encodeAsBooleanFormula(constraint)) return boost::none;

		FormulasT result;

		Rational constant = -constraint.constantPart();
		for (unsigned i = 0 ; i <= constant; i++) {
			result.push_back(FormulaT(encodeExactly(constraint.variables(), i)));
		}

		return FormulaT(carl::FormulaType::OR, result);
	}

	bool CardinalityEncoder::canEncode(const ConstraintT& constraint) {
		bool encodable = true;
		bool allCoeffPositive = true;
		bool allCoeffNegative = true;

		for (const auto& it : constraint.lhs()) {
			if (it.isConstant()) continue;

			encodable = encodable && (it.coeff() == 1 || it.coeff() == -1);
			if (it.coeff() < 0) allCoeffPositive = false;
			if (it.coeff() > 0) allCoeffNegative = false;
		}

		encodable = encodable && allCoeffNegative != allCoeffPositive;

		return encodable;
	}

	Rational factorial(Rational n);
	Rational factorial(std::size_t);

	Rational CardinalityEncoder::encodingSize(const ConstraintT& constraint) {

		SMTRAT_LOG_DEBUG("smtrat.pbc", "Calculating encodingSize for Cardinality.");

		std::size_t nVars = constraint.variables().size();
		Rational constantPart = carl::abs(constraint.constantPart());

		Rational binom = factorial(nVars)/(factorial(constantPart) * factorial(nVars - constantPart));

		return binom;
	}

	Rational factorial(std::size_t n) {
		return factorial(Rational(n));
	}

	Rational factorial(Rational n) {
		Rational res = 1;
		for (Rational i = 1; i < n; i++) {
			res = res * i;
		}

		return res;
	}
}

