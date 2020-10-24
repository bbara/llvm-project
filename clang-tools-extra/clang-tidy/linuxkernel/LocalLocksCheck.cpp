//===--- LocalLocksCheck.cpp - clang-tidy ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LocalLocksCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/TokenKinds.h>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace linuxkernel {
namespace {
/* look up table, context-less -> <local lock, has param> */
static std::map<std::string, std::tuple<std::string, bool>> const LUT = {
    {"preempt_disable", {"local_lock", false}},
    {"preempt_enable", {"local_unlock", false}},
    {"local_irq_disable", {"local_lock_irq", false}},
    {"local_irq_enable", {"local_unlock_irq", false}},
    {"local_irq_save", {"local_lock_irqsave", true}},
    {"local_irq_restore", {"local_unlock_irqrestore", true}},
};

static bool const FixHeaderActive = false;
static bool const FixInitActive = false;
static bool const FixFieldActive = false;
static bool const FixFunctionsActive = false;

class LocalLocksPPCallbacks : public PPCallbacks {
public:
  LocalLocksPPCallbacks(LocalLocksCheck *Check, SourceManager const &SM)
      : Check(Check), SM(&SM) {}

  void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD,
                    SourceRange Range, const MacroArgs *Args) override {
    /* store all calls from local file to context-less lock functions */
    if (!SM->isInMainFile(MacroNameTok.getLocation())) {
      return;
    }
    const auto *Identifier = MacroNameTok.getIdentifierInfo();
    const auto LUTEntry = LUT.find(Identifier->getNameStart());
    if (LUTEntry != LUT.end()) {
      const auto Loc = MacroNameTok.getLocation();
      const auto OriginalName = std::string(Identifier->getNameStart());
      Check->addLockCall(Loc, OriginalName);
    }
  }

  void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange FilenameRange, const FileEntry *File,
                          StringRef SearchPath, StringRef RelativePath,
                          const Module *Imported,
                          SrcMgr::CharacteristicKind FileType) override {
    /* find an include and check if local_locks is already included.
     * the include should be used as location for the local lock header.
     */
    if (!SM->isInMainFile(HashLoc)) {
      return;
    }
    Check->setIncludeLoc(HashLoc);
    if (FileName.equals("linux/local_lock.h")) {
      Check->setAlreadyIncluded();
    }
  }

private:
  LocalLocksCheck *Check;
  SourceManager const *SM;
};
} // namespace

void LocalLocksCheck::registerPPCallbacks(const SourceManager &SM,
                                          Preprocessor *PP,
                                          Preprocessor *ModuleExpanderPP) {
  PP->addPPCallbacks(std::make_unique<LocalLocksPPCallbacks>(this, SM));
  this->SM = &SM;
}

void LocalLocksCheck::addLockCall(SourceLocation Loc, std::string Original) {
  this->Locks[Loc] = Original;
}

void LocalLocksCheck::setIncludeLoc(SourceLocation Loc) { IncludeLoc = Loc; }

void LocalLocksCheck::setAlreadyIncluded() { AlreadyIncluded = true; }

void LocalLocksCheck::registerMatchers(MatchFinder *Finder) {
  /* a struct pointer in the caller function's params can be used as context */
  auto StructParam = recordDecl(isStruct()).bind("struct");
  auto Param = parmVarDecl(hasType(pointsTo(StructParam))).bind("name");
  auto CallerFn = functionDecl(hasAnyParameter(Param)).bind("funDecl");
  Finder->addMatcher(CallerFn, this);

  /* TODO: in case of a forward decl, we don't get the complete easily */
  auto StructDecl = recordDecl(isDefinition(), isStruct()).bind("def");
  Finder->addMatcher(StructDecl, this);
}

void LocalLocksCheck::diagMissingHeader(SourceLocation const Loc) {
  FixItHint Hint;
  if (FixHeaderActive) {
    Hint = FixItHint::CreateInsertion(Loc, "#include <linux/local_lock.h>\n");
  }
  diag(Loc, "missing local lock header") << Hint;
}

std::string LocalLocksCheck::findLocalLockStructField(RecordDecl const *Rec) {
  for (auto Iter = Rec->field_begin(); Iter != Rec->field_end(); Iter++) {
    const auto FieldType = Iter->getType().getAsString();
    if (FieldType == "local_lock_t") {
      return Iter->getNameAsString();
    }
  }
  return "";
}

std::string LocalLocksCheck::diagMissingField(RecordDecl const *Rec) {
  FixItHint Hint;
  if (FixFieldActive) {
    const auto Loc = Rec->getEndLoc().getLocWithOffset(-1);
    Hint = FixItHint::CreateInsertion(Loc, "\n\tlocal_lock_t local_lock;");
  }
  diag(Rec->getBeginLoc(), "missing local lock field") << Hint;
  return "local_lock";
}

void LocalLocksCheck::diagLockCall(std::string Ctx, std::string Field,
                                   SourceLocation Loc, std::string Original) {
  const auto LUTEntry = LUT.at(Original);
  const auto ReplFun = std::get<0>(LUTEntry);
  const auto *Optional = std::get<1>(LUTEntry) ? ", " : "";

  const auto Length = Original.size();
  const auto Range = SourceRange(Loc, Loc.getLocWithOffset(Length));
  FixItHint Hint;
  if (FixFunctionsActive) {
    auto Replacement = ReplFun + "(" + Ctx + "->" + Field + Optional;
    Hint = FixItHint::CreateReplacement(Range, Replacement);
  }
  diag(Loc, "'%0' can be used") << ReplFun << Hint;
}

void LocalLocksCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *StructDecl = Result.Nodes.getNodeAs<RecordDecl>("def");
  if (StructDecl) {
    auto StructName = StructDecl->getNameAsString();
    this->StructDecls[StructName] = StructDecl;
  }
  const auto *FunDecl = Result.Nodes.getNodeAs<FunctionDecl>("funDecl");
  const auto *CtxType = Result.Nodes.getNodeAs<RecordDecl>("struct");
  if (this->Locks.empty() || !FunDecl || !CtxType)
    return;

  /* get all context-less lock function calls from this function */
  const auto LowerIter = this->Locks.lower_bound(FunDecl->getBeginLoc());
  const auto UpperIter = this->Locks.upper_bound(FunDecl->getEndLoc());
  if (LowerIter == UpperIter) {
    /* no lock function call found */
    return;
  }

  /* ensure that local lock header is included, but only once */
  if (!this->AlreadyIncluded && this->IncludeLoc.isValid()) {
    diagMissingHeader(this->IncludeLoc);
    setAlreadyIncluded();
  }

  /* check if the context has already a local lock field */
  const auto *StructDef = CtxType->getDefinition();
  if (!StructDef)
    return;
  const auto StructName = CtxType->getNameAsString();

  /* workaround: struct forward decls doesn't help us */
  const auto StructDefIter = this->StructDecls.find(StructName);
  if (StructDefIter == this->StructDecls.end())
    return;
  StructDef = StructDefIter->second;

  const auto FieldIter = this->StructToFieldMap.find(StructName);
  std::string FieldName;
  if (FieldIter == this->StructToFieldMap.end()) {
    FieldName = this->findLocalLockStructField(StructDef);
  }
  if (FieldName.empty()) {
    FieldName = diagMissingField(StructDef);
  }
  this->StructToFieldMap[StructName] = FieldName;

  /* do not replace stuff in not local location */
  if (!this->SM->isInMainFile(FunDecl->getLocation())) {
    return;
  }

  /* context(s) used in file, but function has no respective parameter */
  auto Iter = this->Locks.begin();
  for (; Iter != LowerIter && Iter != this->Locks.end(); Iter++) {
    const auto Replacement = std::get<0>(LUT.at(Iter->second));
    diag(Iter->first, "'%0' could be used") << Replacement;
  }
  this->Locks.erase(this->Locks.begin(), Iter);

  /* function has context parameter */
  const auto *CtxName = Result.Nodes.getNodeAs<ParmVarDecl>("name");
  for (Iter = LowerIter; Iter != UpperIter && Iter != Locks.end(); Iter++) {
    const auto Loc = Iter->first;
    const auto OriginalName = Iter->second;
    diagLockCall(CtxName->getNameAsString(), FieldName, Loc, OriginalName);
  }
  this->Locks.erase(LowerIter, Iter);

  /* TODO: handle the param-less, ctx-less locks between last upper and end */
}

} // namespace linuxkernel
} // namespace tidy
} // namespace clang
