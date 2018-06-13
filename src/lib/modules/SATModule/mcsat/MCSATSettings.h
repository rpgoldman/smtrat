#pragma once

#include "../../../datastructures/mcsat/arithmetic/AssignmentFinder_arithmetic.h"
#include "../../../datastructures/mcsat/explanations/ParallelExplanation.h"
#include "../../../datastructures/mcsat/explanations/SequentialExplanation.h"
#include "../../../datastructures/mcsat/fm/Explanation.h"
#include "../../../datastructures/mcsat/nlsat/Explanation.h"
#include "../../../datastructures/mcsat/vs/Explanation.h"
#include "../../../datastructures/mcsat/variableordering/VariableOrdering.h"

namespace smtrat {
namespace mcsat {

struct MCSATSettingsNL {
	static constexpr VariableOrdering variable_ordering = VariableOrdering::FeatureBased;
	using AssignmentFinderBackend = arithmetic::AssignmentFinder;
	using ExplanationBackend = SequentialExplanation<nlsat::Explanation>;
};

struct MCSATSettingsFMNL {
	static constexpr VariableOrdering variable_ordering = VariableOrdering::FeatureBased;
	using AssignmentFinderBackend = arithmetic::AssignmentFinder;
	using ExplanationBackend = SequentialExplanation<fm::Explanation,nlsat::Explanation>;
};

struct MCSATSettingsVSNL {
	static constexpr VariableOrdering variable_ordering = VariableOrdering::FeatureBased;
	using AssignmentFinderBackend = arithmetic::AssignmentFinder;
	using ExplanationBackend = SequentialExplanation<vs::Explanation,nlsat::Explanation>;
};

struct MCSATSettingsFMVSNL {
	static constexpr VariableOrdering variable_ordering = VariableOrdering::FeatureBased;
	using AssignmentFinderBackend = arithmetic::AssignmentFinder;
	using ExplanationBackend = SequentialExplanation<fm::Explanation,vs::Explanation,nlsat::Explanation>;
};

}
}