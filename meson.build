project('smt-tc-checker', 'cpp', version: '0.0.1')

cmake = import('cmake')

z3_opts = cmake.subproject_options()
z3_opts.add_cmake_defines({'Z3_ENABLE_EXAMPLE_TARGETS': 'OFF'}) # workaround meson bug
z3_pro = cmake.subproject('z3', options: z3_opts)
z3_dep = declare_dependency(
  include_directories: include_directories('subprojects/z3/src/api/c++'),
  dependencies: z3_pro.dependency('libz3')
)

executable(
  'checker',
  'src/main.cpp',
  dependencies: [z3_dep],
  override_options: ['cpp_std=c++20']
)
