(set-logic QF_NRA)
(declare-fun r () Real)
(declare-fun x () Real)
(declare-fun y () Real)
(assert (and (> (- x r) 0) (implies (> (- y r) 0) (> (- (* (* x x) (* (+ 1 (* 2 y)) (+ 1 (* 2 y)))) (* (* y y) (+ 1 (* 2 (* x x))))) 0))))
(eliminate-quantifiers (exists r) (forall x y))
(exit)