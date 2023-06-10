# Docs for Contributers

## Related Material

- Papers
  - [Intro to SAT/SMT](papers/smt-intro.pdf)
  - [Cobra](papers/cobra.pdf)
  - [Zord](papers/zord.pdf)
- Z3
  - [Intro to Z3 and UserPropagator](http://theory.stanford.edu/~nikolaj/z3navigate.html)
  - [UserPropagator C++ API](https://z3prover.github.io/api/html/classz3_1_1user__propagator__base.html)
  - [Example](https://github.com/Z3Prover/z3/tree/master/examples/userPropagator)

## Project Structure

![](assets/algo-structure.svg)

The entire checking procedure is split into four parts: parsing, encoding, SMT
solving and theory solving. The input of this solver is a history file generated
by [dbcop](https://github.com/amnore/dbcop), and the output is whether the
history is Serializable (accept) or not (reject).

- Parsing: Parse the collected history from a file, then infer known WR
  dependencies (known graph) and constraints from the history.
- Encoding: Encode the known graph and constraints into SAT formulae, and
  initialize theory solver.
- SMT Solving: Handled by Z3's solver.
- Theory Solving: Track the dependency graph resulted from Z3's variable
  assignments, and enforces Serializability constraint by checking that the
  dependency graph is cycle-free.

TODO: algo-details
