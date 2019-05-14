namespace smtrat {
    class VarSchedulerBase;
    template<mcsat::VariableOrdering vot>
    class TheoryVarSchedulerStatic;

    class VarSchedulerMinisat;
    class VarSchedulerRandom;
    class VarSchedulerFixedRandom;
    
    template<mcsat::VariableOrdering vot>
    class VarSchedulerMcsatBooleanFirst;
    //template<mcsat::VariableOrdering vot>
    template<typename TheoryScheduler>
    class VarSchedulerMcsatTheoryFirst;
    template<mcsat::VariableOrdering vot>
    class VarSchedulerMcsatActivityPreferTheory;
    template<int lookahead, mcsat::VariableOrdering vot>
    class VarSchedulerMcsatUnivariateConstraintsOnly;
}