#include <iostream>
#include "gtest/gtest.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8_test_fixture.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/parsing.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/common/globals.h"

using namespace v8;
namespace i = v8::internal;

class AstTest : public V8TestFixture {
};

TEST_F(AstTest, ParseInfo) {
  const Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Factory* factory = internal_isolate->factory();
  i::Handle<i::String> source = factory->NewStringFromStaticChars(
      "const msg = 'testing'");
  i::Handle<i::Script> script = factory->NewScript(source);
  i::UnoptimizedCompileState state(internal_isolate);
  i::UnoptimizedCompileFlags compile_flags = i::UnoptimizedCompileFlags::ForScriptCompile(
      internal_isolate, *script);
  i::ParseInfo parse_info(internal_isolate, compile_flags, &state);
  EXPECT_TRUE(compile_flags.is_toplevel());
  EXPECT_FALSE(compile_flags.is_eval());
  //EXPECT_FALSE(parse_info.is_strict_mode());
  //EXPECT_FALSE(compile_flags.is_native());
  EXPECT_FALSE(compile_flags.is_module());
  i::Scope* scope = parse_info.literal()->scope();
  EXPECT_EQ(scope->start_position(), 0) << "start line (I think) was not 0";
  EXPECT_EQ(scope->end_position(), 0) << "end line (I think) was not 0";
}

TEST_F(AstTest, Parser) {
  const Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  i::Isolate* internal_isolate = asInternal(isolate_);
  i::Factory* factory = internal_isolate->factory();
  i::Handle<i::String> source = factory->NewStringFromStaticChars(
      "const msg = 'testing'");
  i::Handle<i::Script> script = factory->NewScript(source);
  i::UnoptimizedCompileState state(internal_isolate);
  i::UnoptimizedCompileFlags compile_flags = i::UnoptimizedCompileFlags::ForScriptCompile(
      internal_isolate, *script);
  i::ParseInfo parse_info(internal_isolate, compile_flags, &state);
  bool result = i::parsing::ParseProgram(&parse_info,
      script, internal_isolate, i::parsing::ReportStatisticsMode::kYes);
  EXPECT_TRUE(result);
  // ParseProgram set parse_info's literal_ which is a AST node
  i::FunctionLiteral* function_literal = parse_info.literal();
  EXPECT_EQ(function_literal->syntax_kind(), i::FunctionSyntaxKind::kAnonymousExpression);
  EXPECT_EQ(function_literal->kind(), i::FunctionKind::kNormalFunction);
}
