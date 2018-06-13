#pragma once

#include "PseudoBoolEncoder.h"
#include "LongFormulaEncoder.h"

namespace smtrat {
	class MixedSignEncoder : public PseudoBoolEncoder {
		public:
			MixedSignEncoder() : PseudoBoolEncoder () {}

		protected:
			boost::optional<FormulaT> doEncode(const ConstraintT& constraint);

		private:
			LongFormulaEncoder mLongFormulaEncoder;

	};
}