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
  // test spaces, order of precedence
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

TEST(TestQuery, ExpectedThrowsBuildQueryExpr){
    // Throws because consecutive operator
    EXPECT_THROW(helib::buildQuery("0 AND AND 1"),helib::LogicError);
    // Throws because missing space before AND
    EXPECT_THROW(helib::buildQuery("0AND 1"),helib::LogicError);
    // Throws because expects integer columns
    EXPECT_THROW(helib::buildQuery(" x AND y AND z"),helib::LogicError);
    // Throws because query badly formed (final operand stack is non empty)
    EXPECT_THROW(helib::buildQuery("(0 AND 1"),helib::LogicError);
    // Throws because query badly formed (final operand stack is non empty)
    EXPECT_THROW(helib::buildQuery("(0 AND 1) 2"),helib::LogicError);
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

  // Multi-width columns
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

  // Multi-word columns

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

TEST(TestQuery, ExpectedThrowsQueryExprFromTable)
{
  // a test for every exception that can be thrown in the Table class

  // throws because number of columns does not equal number of dimensions
  EXPECT_THROW(helib::Table({"col1", "col2"}, {1, 2, 3}), helib::LogicError);
  helib::Table t({"name", "age", "weight"});

  // throws because trying to resize a column to a negative number
  EXPECT_THROW(t.resize(0, -2), helib::LogicError);

  // throws because query badly formed (final size of operator stack is 1)
  EXPECT_THROW(t.buildQueryExpr("(name AND age"), helib::LogicError);
  // throws because query badly formed (final size of operand stack is 2)
  EXPECT_THROW(t.buildQueryExpr("(name AND age) weight"), helib::LogicError);
  // TODO: EXPECT_THROW(t.buildQueryExpr("name AND age AND"),helib::LogicError);
  // segfaults rather than throws

  // throws because unrecognised column
  EXPECT_THROW(t.buildQueryExpr("name AND height"), helib::LogicError);
  // throws because unrecognised column "name age"
  EXPECT_THROW(t.buildQueryExpr("name age AND weight"), helib::LogicError);
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

  // Multi-width columns
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

  // Multi-word columns
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

TEST(TestQuery, TestMakeTableFromString1DColumns)
{
  helib::Table t0 = helib::makeTableFromString("Table(name,age,weight)");
  std::vector<std::string> expected_col_names = {"name", "age", "weight"};
  std::vector<long> expected_col_dims = {1, 1, 1};
  EXPECT_EQ(t0.getColNames(), expected_col_names);
  EXPECT_EQ(t0.getColDims(), expected_col_dims);
  EXPECT_EQ(t0.getColCount(), 3);

  helib::Table t1 = helib::makeTableFromString("Table(first name, last name, date of birth)");
  expected_col_names = {"first name", "last name", "date of birth"};
  expected_col_dims = {1, 1, 1};
  EXPECT_EQ(t1.getColNames(), expected_col_names);
  EXPECT_EQ(t1.getColDims(), expected_col_dims);
  EXPECT_EQ(t1.getColCount(), 3);
}

TEST(TestQuery, ExpectedThrowsTableFromString){
    // Throws because string does not start with Table(
    EXPECT_THROW(helib::makeTableFromString("Tab(x,y,z)"),helib::LogicError);
    // Throws because string does not end with )
    EXPECT_THROW(helib::makeTableFromString("Table(x,y,z"),helib::LogicError);
    // throws because space between table and first bracket
    EXPECT_THROW(helib::makeTableFromString("Table (x,y,z)"),helib::LogicError);
}

} // namespace
