// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <helib/helib.h>
#include <helib/query.h>

#include "gtest/gtest.h"
#include "test_common.h"

namespace {

TEST(TestQuery, QueryBuilderFromStringInfixToPolishConversion)
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

TEST(TestQuery, ExpectedThrowsInfixToPostFixNotation)
{
  // throws because left bracket next to AND
  EXPECT_THROW(helib::QueryBuilder("(AND a OR b"), helib::LogicError);
  // throws because OR next to AND
  EXPECT_THROW(helib::QueryBuilder("c OR AND b"), helib::LogicError);
  // throws because left bracket next to right bracket
  EXPECT_THROW(helib::QueryBuilder("() c AND b"), helib::LogicError);
  // throws because no spaces around AND
  EXPECT_THROW(helib::QueryBuilder("aAND c OR b"), helib::LogicError);
  // throws because no spaces around OR
  EXPECT_THROW(helib::QueryBuilder("c ORb"), helib::LogicError);

  // TODO: uncomment these when extra query checks added
  /**
  // throws because consecutive operands
  EXPECT_THROW(helib::QueryBuilder("a b c"),helib::LogicError);
  EXPECT_THROW(helib::QueryBuilder("date of birth AND c"),helib::LogicError);
  // throws because unequal number of left and right brackets
  EXPECT_THROW(helib::QueryBuilder("(a AND b))"),helib::LogicError);
  EXPECT_THROW(helib::QueryBuilder("((b)"),helib::LogicError);
  // throws because expression starts with AND
  EXPECT_THROW(helib::QueryBuilder("AND a AND b"),helib::LogicError);
  // throws because expression starts with OR
  EXPECT_THROW(helib::QueryBuilder("y OR "),helib::LogicError);
  **/
}

TEST(TestQuery, BuildQueryExprFromInfixString)
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
  std::vector<std::string> expected;
  expected.push_back("0 1 &&");
  expected.push_back("0 1 ||");
  expected.push_back("1 2 ||");
  expected.push_back("0 1 2 || &&");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 3 && 4 5 || && ||");
  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].exp->eval(), expected[i]) << "*** i= " << i;
  }
}

TEST(TestQuery, BuildQueryExprFromInfixStringSpacesAndPrecedence)
{
  std::vector<helib::QueryExpr> input_exprs;
  input_exprs.push_back(helib::buildQuery("0 AND 1 AND 2"));
  input_exprs.push_back(helib::buildQuery("( 0 OR 1 AND 2 )"));
  input_exprs.push_back(helib::buildQuery("0 AND 1 OR 2"));
  input_exprs.push_back(
      helib::buildQuery("( 0 AND 1 ) OR ( 2 AND 3 ) AND ( 4 OR 5 )"));
  std::vector<std::string> expected;
  expected.push_back("0 1 && 2 &&");
  expected.push_back("0 1 2 && ||");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("0 1 && 2 3 && 4 5 || && ||");
  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].exp->eval(), expected[i]) << "*** i= " << i;
  }
}

TEST(TestQuery, BuildQueryExprFromInfixStringBrackets)
{
  std::vector<helib::QueryExpr> input_exprs;
  input_exprs.push_back(helib::buildQuery("(0 AND 1) OR (2 AND 3)"));
  input_exprs.push_back(helib::buildQuery("(0 AND (1 AND (2 AND 3)))"));
  std::vector<std::string> expected;
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 2 3 && && &&");
  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].exp->eval(), expected[i]) << "*** i= " << i;
  }
}

TEST(TestQuery, ExpectedThrowsBuildQueryExpr)
{
  // Throws because consecutive operator
  EXPECT_THROW(helib::buildQuery("0 AND AND 1"), helib::LogicError);
  // Throws because missing space before AND
  EXPECT_THROW(helib::buildQuery("0AND 1"), helib::LogicError);
  // Throws because expects integer columns
  EXPECT_THROW(helib::buildQuery(" x AND y AND z"), helib::LogicError);
  // Throws because query badly formed (final operand stack is non empty)
  EXPECT_THROW(helib::buildQuery("(0 AND 1"), helib::LogicError);
  // Throws because query badly formed (final operand stack is non empty)
  EXPECT_THROW(helib::buildQuery("(0 AND 1) 2"), helib::LogicError);
  // Throws because query badly formed (final operand stack is non empty)
  EXPECT_THROW(helib::buildQuery("0  1 AND 2"), helib::LogicError);
  // Throws because query badly formed (trailing ANDs or ORs)
  EXPECT_THROW(helib::buildQuery("0 AND 1 AND"), helib::LogicError);
  EXPECT_THROW(helib::buildQuery("AND 0 AND 1"), helib::LogicError);
  EXPECT_THROW(helib::buildQuery("1 OR"), helib::LogicError);
}

// TODO: This requires a rethink because std::unordered_map is not ordered the
// string is not guaranteed to print in the same order.
TEST(TestQuery, TableFromString1DColumns)
{
  helib::Table t("TABLE(name,age,weight)");
  std::ostringstream tablestring;
  t.printTable(tablestring);
  std::string expected_tablestring =
      "Key:[name] Value:[0,1]\nKey:[age] Value:[1,1]\nKey:[weight] Value:[2,1]";
  EXPECT_EQ(tablestring.str(), expected_tablestring);
  // TODO: add extra tests?
  /**
  helib::Table t1("TABLE (name,age,weight)");
  std::ostringstream tablestring;
  t1.printTable(tablestring);
  std::string expected_tablestring =
      "Key:[name] Value:[0,1]\nKey:[age] Value:[1,1]\nKey:[weight] Value:[2,1]";
  EXPECT_EQ(tablestring.str(), expected_tablestring);
  **/
}

/**
TEST(TestQuery, TableFromStringMultiDimensionalColumns) {
  // TODO
  helib::Table t0("TABLE(name (2) ,age (3),weight (2))");
  std::ostringstream tablestring;
  t.printTable(tablestring);
  std::string expected_tablestring =
      "Key:[name] Value:[0,2]\nKey:[age] Value:[2,3]\nKey:[weight] Value:[5,2]";
  EXPECT_EQ(tablestring.str(), expected_tablestring);

  helib::Table t("TABLE(name,age (4),weight(1))");
  std::ostringstream tablestring;
  t.printTable(tablestring);
  std::string expected_tablestring =
      "Key:[name] Value:[0,1]\nKey:[age] Value:[1,4]\nKey:[weight] Value:[5,1]";
  EXPECT_EQ(tablestring.str(), expected_tablestring);

}
**/

TEST(TestQuery, ExpectedThrowsTableFromString)
{
  // Throws because string does not start with TABLE(
  EXPECT_THROW(helib::Table("Tab(x,y,z)"), helib::LogicError);
  // Throws because string does not end with )
  EXPECT_THROW(helib::Table("TABLE(x,y,z"), helib::LogicError);
  // TODO: throw for repeated column? Yes
  // EXPECT_THROW(helib::Table(TABLE(x,y,x)), helib::LogicError);
  // TODO: throw for multiword column? Yes, need to add check for this
  // TODO: Throw for brackets in column name? Yes
  // TODO: throw for column name with AND or OR? Yes (and NOT)
  // TODO: Throw for non-positive dimensions
  // TODO: Throw for dimensions that are not numbers (I think this needs to be
  // added to the code)
}

TEST(TestQuery, BuildQueryString1DColumns)
{
  helib::Table t("TABLE(name,age,weight,height)");
  std::vector<std::string> input_exprs;
  input_exprs.push_back(t.buildQueryString("name AND age"));
  input_exprs.push_back(t.buildQueryString("name OR age"));
  input_exprs.push_back(t.buildQueryString("( age OR weight )"));
  input_exprs.push_back(t.buildQueryString("name OR age AND weight"));
  input_exprs.push_back(t.buildQueryString("( name OR age ) AND weight"));
  input_exprs.push_back(
      t.buildQueryString("( name AND age ) OR ( weight AND height )"));
  input_exprs.push_back(
      t.buildQueryString("(name AND age AND weight AND height)"));
  std::vector<std::string> expected;
  expected.push_back("0 AND 1 ");
  expected.push_back("0 OR 1 ");
  expected.push_back("( 1 OR 2 ) ");
  expected.push_back("0 OR 1 AND 2 ");
  expected.push_back("( 0 OR 1 ) AND 2 ");
  expected.push_back("( 0 AND 1 ) OR ( 2 AND 3 ) ");
  expected.push_back("( 0 AND 1 AND 2 AND 3 ) ");
  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i], expected[i]) << "*** i= " << i;
  }
}

/**
TEST(TestQuery, BuildQueryStringMultidimensionalColumns) {}
**/

/**
TEST(TestQuery, ExpectedThrowsBuildQueryString) {
  
}
**/

TEST(TestQuery, BuildQueryBuilderFromTable1DColumns)
{
  helib::Table t("TABLE(name, age, weight),height)");
  std::vector<helib::QueryBuilder> input_exprs;
  std::vector<std::string> expected;
  input_exprs.push_back(
      helib::QueryBuilder(t.buildQueryString("name AND age")));
  input_exprs.push_back(helib::QueryBuilder(t.buildQueryString("name OR age")));
  input_exprs.push_back(
      helib::QueryBuilder(t.buildQueryString("( age OR weight )")));
  input_exprs.push_back(
      helib::QueryBuilder(t.buildQueryString("name OR age AND weight")));
  input_exprs.push_back(
      helib::QueryBuilder(t.buildQueryString("( name OR age ) AND weight")));
  input_exprs.push_back(helib::QueryBuilder(
      t.buildQueryString("( name AND age ) OR ( weight AND height )")));
  input_exprs.push_back(helib::QueryBuilder(
      t.buildQueryString("(name AND age AND weight AND height)")));
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
/**
TEST(TestQuery, BuildQueryBuilderFromTableMultidimensionalColumns){
  // Multi-width columns
  helib::Table t1({"country", "capital", "population", "currency"},
                  {2, 1, 1, 2});
  input_exprs.push_back(
      QueryBuilder(t1.buildQueryString("country AND capital")));
  input_exprs.push_back(
      QueryBuilder(t1.buildQueryString("country OR capital")));
  input_exprs.push_back(
      QueryBuilder(t1.buildQueryString("( capital OR population )")));
  input_exprs.push_back(
      QueryBuilder(t1.buildQueryString("country OR capital AND population")));
  input_exprs.push_back(QueryBuilder(
      t1.buildQueryString("( country OR capital ) AND population")));
  input_exprs.push_back(QueryBuilder(t1.buildQueryString(
      "( country AND capital ) OR ( population AND currency )")));
  input_exprs.push_back(QueryBuilder(t1.buildQueryString(
      "(country AND capital AND population AND currency)")));
  expected.push_back("0 1 && 2 &&");
  expected.push_back("0 1 && 2 ||");
  expected.push_back("2 3 ||");
  expected.push_back("0 1 && 2 3 && ||");
  expected.push_back("0 1 && 2 || 3 &&");
  expected.push_back("0 1 && 2 && 3 4 && 5 && ||");
  expected.push_back("0 1 && 2 && 3 && 4 && 5 &&");
  for (size_t i = 0; i < input_exprs.size(); ++i) {
    EXPECT_EQ(input_exprs[i].getQueryString(), expected[i]) << "*** i= " << i;
  }
}
**/

/**
TEST(TestQuery, ExpectedThrowsBuildQueryBuilderFromTable)
{
  // function calls buildQueryString and then the query builder from string
  // constructor, so essentially a rehash of earlier tests
}**/
} // namespace
