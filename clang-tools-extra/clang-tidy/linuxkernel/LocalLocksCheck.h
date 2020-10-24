//===--- LocalLocksCheck.h - clang-tidy -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_LINUXKERNEL_LOCALLOCKSCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_LINUXKERNEL_LOCALLOCKSCHECK_H

#include "../ClangTidyCheck.h"
#include <clang/Basic/IdentifierTable.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

namespace clang {
namespace tidy {
namespace linuxkernel {

/// Checks Linux kernel code to see if it uses context-less functions to
/// disable/enable preemption and IRQs. Instead, suggests the respective local
/// counterpart, located in <linux/local_lock.h>.
/// However, the warning only occurs if the caller has a struct pointer param
/// (potential context to store the lock).
///
/// The local locks (available since Linux 5.8) add context to the lock
/// functions, enabling better traceability, documentation and tooling
/// (lockdep).
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/linuxkernel-local-locks.html
class LocalLocksCheck : public ClangTidyCheck {
public:
  LocalLocksCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context), AlreadyIncluded(false) {}
  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

  void addLockCall(SourceLocation Loc, std::string Original);
  void setIncludeLoc(SourceLocation Loc);
  void setAlreadyIncluded();

private:
  void diagMissingHeader(SourceLocation const Loc);
  std::string findLocalLockStructField(RecordDecl const *Rec);
  std::string diagMissingField(RecordDecl const *Rec);
  void diagLockCall(std::string Ctx, std::string Field, SourceLocation Loc,
                    std::string Original);

  std::map<SourceLocation, std::string> Locks;
  std::map<std::string, RecordDecl const *> StructDecls;
  std::map<std::string, std::string> StructToFieldMap;
  SourceLocation IncludeLoc;
  bool AlreadyIncluded;
  SourceManager const *SM;
};

} // namespace linuxkernel
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_LINUXKERNEL_LOCALLOCKSCHECK_H
