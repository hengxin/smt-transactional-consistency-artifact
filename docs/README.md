# Resources
- 相关论文
  - [Intro to SAT/SMT](papers/smt-intro.pdf)
  - [Cobra](papers/cobra.pdf)
  - [Zord](papers/zord.pdf)
- Z3
  - [Intro to Z3 and UserPropagator](http://theory.stanford.edu/~nikolaj/z3navigate.html)
  - [UserPropagator C++ API](https://z3prover.github.io/api/html/classz3_1_1user__propagator__base.html)
  - [Example](https://github.com/Z3Prover/z3/tree/master/examples/userPropagator)
- 项目中的算法、数据结构等
  - [algo-details.pdf](assets/algo-details.pdf)

# RoadMap (按顺序学习)
> 带标注的文献中, 黄色表示需要学习的内容, 红色表示不需要学习的内容
- [Cobra](./annotated-papers/OSDI2020%20Cobra%20Making%20Transactional%20Key-Value%20Stores%20Verifiably%20Serializable.pdf)
- SAT/SMT
  - Theory: [SAT/SMT Course: P1~P5 (The SAT Part)](https://www.bilibili.com/video/BV1Xa4y1e7wT/?share_source=copy_web&vd_source=afddc1f6e07c3046ed07519aa34370fd)
    > 可以与 `Cobra` 并行学习
    > 这是后续学习的理论基础, 尤其是其中的 CDCL 算法。
  - Practice
    > 这部分属于工具学习, 是一个长期积累的过程, 与后面的项目内容穿插着学习)
    - [Z3](https://www.microsoft.com/en-us/research/project/z3-3/)
    - [Z3 @ github](https://github.com/Z3Prover/z3)
    - [SAT SMT by Examples](./annotated-papers/SAT%20SMT%20by%20Example%20(May%202023).pdf)
- [Zord](./annotated-papers/PLDI2021%20Satisfiability%20Modulo%20Ordering%20Consistency%20Theory%20for%20Multi-Threaded%20Program%20Verification.pdf)
- [MonoSAT](./annotated-papers/PhDThesis2010%20SAT%20Modulo%20Monotonic%20Theories.pdf)
- [SAT/SMT Course: P6~ (The SMT Part)](https://www.bilibili.com/video/BV1Xa4y1e7wT/?share_source=copy_web&vd_source=afddc1f6e07c3046ed07519aa34370fd)