// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <helib/helib.h>
#include <helib/query.h>

#include "gtest/gtest.h"
#include "test_common.h"

namespace {

TEST(TestQuery, TestInfixToPolishConversion)
{
  std::vector<helib::QueryBuilder> input_exprs;
  input_exprs.push_back(helib::QueryBuilder("a AND b"));
  input_exprs.push_back(helib::QueryBuilder("a OR b"));
  input_exprs.push_back(helib::QueryBuilder("( b OR c )"));
  input_exprs.push_back(helib::QueryBuilder("a AND ( b OR c )"));
  input_exprs.push_back(helib::QueryBuilder("( a AND b ) OR c"));
  input_exprs.push_back(helib::QueryBuilder("( a AND b ) OR ( c AND d )"));
  input_exprs.push_back(
      helib::QueryBuilder("( a AND b ) OR ( ( c AND d ) AND ( e OR f ) )"));
  // test spaces, order of precedence, triple char operands
  input_exprs.push_back(helib::QueryBuilder("a AND b AND c"));
  input_exprs.push_back(helib::QueryBuilder("( a OR b AND c )"));
  input_exprs.push_back(helib::QueryBuilder("a AND b OR c"));
  input_exprs.push_back(helib::QueryBuilder("aaa OR bbc OR ac"));
  input_exprs.push_back(
      helib::QueryBuilder("( a AND b ) OR ( c AND d ) AND ( e OR f )"));
  // test brackets
  input_exprs.push_back(helib::QueryBuilder("(a AND b) OR (c AND d)"));
  input_exprs.push_back(helib::QueryBuilder("(a AND (b AND (c AND d)))"));
  std::vector<std::string> expected;
  expected.push_back("a b &&");
  expected.push_back("a b ||");
  expected.push_back("b c ||");
  expected.push_back("a b c || &&");
  expected.push_back("a b && c ||");
  expected.push_back("a b && c d && ||");
  expected.push_back("a b && c d && e f || && ||");
  expected.push_back("a b && c &&");
  expected.push_back("a b c && ||");
  expected.push_back("a b && c ||");
  expected.push_back("aaa bbc || ac ||");
  expected.push_back("a b && c d && e f || && ||");
  expected.push_back("a b && c d && ||");
  expected.push_back("a b c d && && &&");

  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].getQueryString(), expected[i]) << "*** i = " << i;
  }
}

TEST(TestQuery, TestBuildQueryExprFromInfixString)
{
  std::vector<helib::QueryExpr> input_exprs;
  input_exprs.push_back(helib::buildQuery("0 AND 1"));
  input_exprs.push_back(helib::buildQuery("0 OR 1"));
  input_exprs.push_back(helib::buildQuery("( 1 OR 2 )"));
  input_exprs.push_back(helib::buildQuery("0 AND ( 1 OR 2 )"));
  input_exprs.push_back(helib::buildQuery("( 0 AND 1 ) OR 2"));
  input_exprs.push_back(helib::buildQuery("( 0 AND 1 ) OR ( 2 AND 3 )"));
  input_exprs.push_back(
      helib::buildQuery("( 0 AND 1 ) OR ( ( 2 AND 3 ) AND ( 4 OR 5 ) )"));
  // test spaces, order of precedence, triple char operands
  input_exprs.push_back(helib::buildQuery("0 AND 1 AND 2"));
  input_exprs.push_back(helib::buildQuery("( 0 OR 1 AND 2 )"));
  input_exprs.push_back(helib::buildQuery("0 AND 1 OR 2"));
  input_exprs.push_back(
      helib::buildQuery("( 0 AND 1 ) OR ( 2 AND 3 ) AND ( 4 OR 5 )"));
  // test brackets
  input_exprs.push_back(helib::buildQuery("(0 AND 1) OR (2 AND 3)"));
  input_exprs.push_back(helib::buildQuery("(0 AND (1 AND (2 AND 3)))"));
  std::vector<std::string> expected;
  expected.push_back("0 1 &&");
  expected.push_back("0 1 ||");
  expected.push_back("1 2 ||");
  expected.push_back("0 1 2 || &&");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 3 && 4 5 || && ||");
  expected.push_back("0 1 && 2 &&");
  expected.push_back("0 1 2 && ||");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("0 1 && 2 3 && 4 5 || && ||");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 2 3 && && &&");

  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].exp->eval(), expected[i]) << "*** i= " << i;
  }
}

TEST(TestQuery, TestBuildQueryExprFromTable)
{
  // width 1 columns
  helib::Table t0({"name", "age", "weight"});
  t0.append("height");
  std::vector<helib::QueryExpr> input_exprs;
  std::vector<std::string> expected;
  input_exprs.push_back(t0.buildQueryExpr("name AND age"));
  input_exprs.push_back(t0.buildQueryExpr("name OR age"));
  input_exprs.push_back(t0.buildQueryExpr("( age OR weight )"));
  input_exprs.push_back(t0.buildQueryExpr("name OR age AND weight"));
  input_exprs.push_back(t0.buildQueryExpr("( name OR age ) AND weight"));
  input_exprs.push_back(
      t0.buildQueryExpr("( name AND age ) OR ( weight AND height )"));
  input_exprs.push_back(
      t0.buildQueryExpr("(name AND age AND weight AND height)"));
  expected.push_back("0 1 &&");
  expected.push_back("0 1 ||");
  expected.push_back("1 2 ||");
  expected.push_back("0 1 2 && ||");
  expected.push_back("0 1 || 2 &&");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 && 3 &&");

  // multiwidth columns
  helib::Table t1({"country", "capital", "population", "currency"},
                  {2, 1, 1, 2});
  input_exprs.push_back(t1.buildQueryExpr("country AND capital"));
  input_exprs.push_back(t1.buildQueryExpr("country OR capital"));
  input_exprs.push_back(t1.buildQueryExpr("( capital OR population )"));
  input_exprs.push_back(t1.buildQueryExpr("country OR capital AND population"));
  input_exprs.push_back(
      t1.buildQueryExpr("( country OR capital ) AND population"));
  input_exprs.push_back(t1.buildQueryExpr(
      "( country AND capital ) OR ( population AND currency )"));
  input_exprs.push_back(
      t1.buildQueryExpr("(country AND capital AND population AND currency)"));
  expected.push_back("0 1 && 2 &&");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("2 3 ||");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 || 3 &&");
  expected.push_back("0 1 && 2 && 3 4 5 && && ||");
  expected.push_back("0 1 && 2 && 3 && 4 5 && &&");

  // multiword columns

  helib::Table t2({"first name", "last name", "date of birth", "address"});
  input_exprs.push_back(t2.buildQueryExpr("first name AND last name"));
  input_exprs.push_back(t2.buildQueryExpr("first name OR last name"));
  input_exprs.push_back(t2.buildQueryExpr("( last name OR date of birth )"));
  input_exprs.push_back(
      t2.buildQueryExpr("first name OR last name AND date of birth"));
  input_exprs.push_back(
      t2.buildQueryExpr("( first name OR last name ) AND date of birth"));
  input_exprs.push_back(t2.buildQueryExpr(
      "( first name AND last name ) OR ( date of birth AND address )"));
  input_exprs.push_back(t2.buildQueryExpr(
      "(first name AND last name AND date of birth AND address)"));
  expected.push_back("0 1 &&");
  expected.push_back("0 1 ||");
  expected.push_back("1 2 ||");
  expected.push_back("0 1 2 && ||");
  expected.push_back("0 1 || 2 &&");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 && 3 &&");

  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].exp->eval(), expected[i]) << "*** i= " << i;
  }
}

TEST(TestQuery, TestBuildQueryBuilderFromTable)
{
  // width 1 columns
  helib::Table t0({"name", "age", "weight"});
  t0.append("height");
  std::vector<helib::QueryBuilder> input_exprs;
  std::vector<std::string> expected;
  input_exprs.push_back(t0.buildQueryBuilder("name AND age"));
  input_exprs.push_back(t0.buildQueryBuilder("name OR age"));
  input_exprs.push_back(t0.buildQueryBuilder("( age OR weight )"));
  input_exprs.push_back(t0.buildQueryBuilder("name OR age AND weight"));
  input_exprs.push_back(t0.buildQueryBuilder("( name OR age ) AND weight"));
  input_exprs.push_back(
      t0.buildQueryBuilder("( name AND age ) OR ( weight AND height )"));
  input_exprs.push_back(
      t0.buildQueryBuilder("(name AND age AND weight AND height)"));
  expected.push_back("0 1 &&");
  expected.push_back("0 1 ||");
  expected.push_back("1 2 ||");
  expected.push_back("0 1 2 && ||");
  expected.push_back("0 1 || 2 &&");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 && 3 &&");

  // multiwidth columns
  helib::Table t1({"country", "capital", "population", "currency"},
                  {2, 1, 1, 2});
  input_exprs.push_back(t1.buildQueryBuilder("country AND capital"));
  input_exprs.push_back(t1.buildQueryBuilder("country OR capital"));
  input_exprs.push_back(t1.buildQueryBuilder("( capital OR population )"));
  input_exprs.push_back(
      t1.buildQueryBuilder("country OR capital AND population"));
  input_exprs.push_back(
      t1.buildQueryBuilder("( country OR capital ) AND population"));
  input_exprs.push_back(t1.buildQueryBuilder(
      "( country AND capital ) OR ( population AND currency )"));
  input_exprs.push_back(t1.buildQueryBuilder(
      "(country AND capital AND population AND currency)"));
  expected.push_back("0 1 && 2 &&");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("2 3 ||");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 || 3 &&");
  expected.push_back("0 1 && 2 && 3 4 && 5 && ||");
  expected.push_back("0 1 && 2 && 3 && 4 && 5 &&");

  helib::Table t2({"first name", "last name", "date of birth", "address"});
  input_exprs.push_back(t2.buildQueryBuilder("first name AND last name"));
  input_exprs.push_back(t2.buildQueryBuilder("first name OR last name"));
  input_exprs.push_back(t2.buildQueryBuilder("( last name OR date of birth )"));
  input_exprs.push_back(
      t2.buildQueryBuilder("first name OR last name AND date of birth"));
  input_exprs.push_back(
      t2.buildQueryBuilder("( first name OR last name ) AND date of birth"));
  input_exprs.push_back(t2.buildQueryBuilder(
      "( first name AND last name ) OR ( date of birth AND address )"));
  input_exprs.push_back(t2.buildQueryBuilder(
      "(first name AND last name AND date of birth AND address)"));
  expected.push_back("0 1 &&");
  expected.push_back("0 1 ||");
  expected.push_back("1 2 ||");
  expected.push_back("0 1 2 && ||");
  expected.push_back("0 1 || 2 &&");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 && 3 &&");

  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].getQueryString(), expected[i]) << "*** i= " << i;
  }
}

} // namespace
