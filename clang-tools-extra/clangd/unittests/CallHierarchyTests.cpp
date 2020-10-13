//===-- CallHierarchyTests.cpp  ---------------------------*- C++ -*-------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "Annotations.h"
#include "Compiler.h"
#include "Matchers.h"
#include "ParsedAST.h"
#include "SyncAPI.h"
#include "TestFS.h"
#include "TestTU.h"
#include "XRefs.h"
#include "index/FileIndex.h"
#include "index/SymbolCollector.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Index/IndexingAction.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ScopedPrinter.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace clang {
namespace clangd {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

// Helpers for matching call hierarchy data structures.
MATCHER_P(WithName, N, "") { return arg.Name == N; }
MATCHER_P(WithSelectionRange, R, "") { return arg.SelectionRange == R; }

template <class ItemMatcher>
::testing::Matcher<CallHierarchyIncomingCall> From(ItemMatcher M) {
  return Field(&CallHierarchyIncomingCall::From, M);
}
template <class... RangeMatchers>
::testing::Matcher<CallHierarchyIncomingCall> FromRanges(RangeMatchers... M) {
  return Field(&CallHierarchyIncomingCall::FromRanges,
               UnorderedElementsAre(M...));
}

TEST(CallHierarchy, IncomingOneFile) {
  Annotations Source(R"cpp(
    void call^ee(int);
    void caller1() {
      $Callee[[callee]](42);
    }
    void caller2() {
      $Caller1A[[caller1]]();
      $Caller1B[[caller1]]();
    }
    void caller3() {
      $Caller1C[[caller1]]();
      $Caller2[[caller2]]();
    }
  )cpp");
  TestTU TU = TestTU::withCode(Source.code());
  auto AST = TU.build();
  auto Index = TU.index();

  llvm::Optional<std::vector<CallHierarchyItem>> Items = prepareCallHierarchy(
      AST, Source.point(), Index.get(), testPath(TU.Filename));
  ASSERT_TRUE(bool(Items));
  EXPECT_THAT(*Items, ElementsAre(WithName("callee")));
  auto IncomingLevel1 = incomingCalls((*Items)[0], Index.get());
  ASSERT_TRUE(bool(IncomingLevel1));
  EXPECT_THAT(*IncomingLevel1,
              ElementsAre(AllOf(From(WithName("caller1")),
                                FromRanges(Source.range("Callee")))));

  auto IncomingLevel2 = incomingCalls((*IncomingLevel1)[0].From, Index.get());
  ASSERT_TRUE(bool(IncomingLevel2));
  EXPECT_THAT(
      *IncomingLevel2,
      UnorderedElementsAre(
          AllOf(From(WithName("caller2")),
                FromRanges(Source.range("Caller1A"), Source.range("Caller1B"))),
          AllOf(From(WithName("caller3")),
                FromRanges(Source.range("Caller1C")))));

  auto IncomingLevel3 = incomingCalls((*IncomingLevel2)[0].From, Index.get());
  ASSERT_TRUE(bool(IncomingLevel3));
  EXPECT_THAT(*IncomingLevel3,
              ElementsAre(AllOf(From(WithName("caller3")),
                                FromRanges(Source.range("Caller2")))));

  auto IncomingLevel4 = incomingCalls((*IncomingLevel3)[0].From, Index.get());
  ASSERT_TRUE(bool(IncomingLevel4));
  EXPECT_THAT(*IncomingLevel4, ElementsAre());
}

TEST(CallHierarchy, IncomingQualified) {
  Annotations Source(R"cpp(
    namespace ns {
    struct Waldo {
      void find();
    };
    void Waldo::find() {}
    void caller1(Waldo &W) {
      W.$Caller1[[f^ind]]();
    }
    void caller2(Waldo &W) {
      W.$Caller2[[find]]();
    }
    }
  )cpp");
  TestTU TU = TestTU::withCode(Source.code());
  auto AST = TU.build();
  auto Index = TU.index();

  llvm::Optional<std::vector<CallHierarchyItem>> Items = prepareCallHierarchy(
      AST, Source.point(), Index.get(), testPath(TU.Filename));
  ASSERT_TRUE(bool(Items));
  EXPECT_THAT(*Items, ElementsAre(WithName("find")));
  auto Incoming = incomingCalls((*Items)[0], Index.get());
  ASSERT_TRUE(bool(Incoming));
  EXPECT_THAT(*Incoming,
              UnorderedElementsAre(AllOf(From(WithName("caller1")),
                                         FromRanges(Source.range("Caller1"))),
                                   AllOf(From(WithName("caller2")),
                                         FromRanges(Source.range("Caller2")))));
}

} // namespace
} // namespace clangd
} // namespace clang
