set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  CodeGen
  Core
  GlobalISel
  MC
  MIRParser
  Support
  Target
  )

add_llvm_unittest(GlobalISelTests
  CSETest.cpp
  LegalizerHelperTest.cpp
  LegalizerInfoTest.cpp
  MachineIRBuilderTest.cpp
  PatternMatchTest.cpp
  )
