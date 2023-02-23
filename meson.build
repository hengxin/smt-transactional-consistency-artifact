project('smt-tc-checker', 'cpp', version: '0.0.1')

cmake = import('cmake')
fs = import('fs')

# z3
z3_dep = dependency('z3', required: false)

if not z3_dep.found()
  z3_opts = cmake.subproject_options()
  z3_opts.add_cmake_defines({
    'Z3_ENABLE_EXAMPLE_TARGETS': 'OFF',
    'Z3_BUILD_EXECUTABLE': 'OFF',
    'Z3_BUILD_TEST_EXECUTABLES': 'OFF'
  })
  if get_option('buildtype') == 'debug'
    z3_opts.add_cmake_defines({
      'CMAKE_BUILD_TYPE': 'Debug'
    })
  endif

  z3_pro = cmake.subproject('z3', options: z3_opts)
  z3_dep = declare_dependency(
    include_directories: include_directories('subprojects/z3/src/api/c++'),
    dependencies: z3_pro.dependency('libz3')
  )
endif

# argparse
argparse_dep = dependency('argparse', required: false)

if not argparse_dep.found()
  argparse_pro = cmake.subproject('argparse')
  argparse_dep = argparse_pro.dependency('argparse')
endif

# boost
boost_dep = dependency('boost', modules: ['log'])

subdir('src')

checker_deps = [z3_dep, argparse_dep, boost_dep]
checker_opts = ['cpp_std=c++20', 'b_sanitize=address']
checker_incs = ['src']

checker_common_deps = declare_dependency(
  dependencies: checker_deps,
  include_directories: checker_incs,
  link_with: [
    static_library(
      'checker_lib',
      checker_srcs,
      dependencies: checker_deps,
      override_options: checker_opts,
      include_directories: checker_incs,
    )
  ],
)

executable(
  'checker',
  files('src/main.cpp'),
  dependencies: checker_common_deps,
  override_options: checker_opts,
)

checker_tests = {}
foreach f : checker_test_srcs
  checker_tests += {
    fs.stem(f) : executable(
      fs.stem(f) + '_test',
      [f],
      dependencies: checker_common_deps,
      override_options: checker_opts,
    )
  }
endforeach

foreach n : ['solver', 'toposort']
  test(n, checker_tests[n])
endforeach
