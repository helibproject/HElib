/* Copyright (C) 2020 IBM Corp.
 * This program is Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. See accompanying LICENSE file.
 */

#ifndef HELIB_PARTIALMATCH_H
#define HELIB_PARTIALMATCH_H

#include <sstream>
#include <stack>
#include <regex>

#include <helib/Matrix.h>
#include <helib/PolyMod.h>

// This code is in flux and should be considered bery alpha.
// Not recommended for public use.

namespace helib {

/**
 * @brief Given a query set and a database, calculates a mask of {0,1} where 1
 * signifies a matching element and 0 otherwise.
 * @tparam TXT type of the query set. Must be a `Ptxt` or `Ctxt`.
 * @param ea The encrypted array object holding information about the scheme.
 * @param query The query set to mask against the database. Must be a row
 * vector of the same dimension as the second dimension of the database matrix.
 * @param database The matrix holding the plaintext database.
 * @return The calculated mask. Is the same size as the database.
 * @note This is an overloaded function for when the database is not encrypted.
 **/
template <typename TXT>
inline Matrix<TXT> calculateMasks(const EncryptedArray& ea,
                                  Matrix<TXT> query,
                                  const Matrix<Ptxt<BGV>>& database)
{
  if (query.dims(0) != 1)
    throw InvalidArgument("Query must be a row vector");
  if (query.dims(1) != database.dims(1))
    throw InvalidArgument(
        "Database and query must have same number of columns");
  // TODO: case where query.dims(0) != database.dims(0)

  // Replicate the query once per row of the database
  // TODO: Some such replication will be needed once blocks/bands exist
  std::vector<long> columns(database.dims(0), 0l);
  Matrix<TXT>& mask = query;
  mask.inPlaceTranspose();
  mask = mask.columns(columns);
  mask.inPlaceTranspose();

  (mask -= database)
      .apply([&](auto& entry) { mapTo01(ea, entry); })
      .apply([](auto& entry) { entry.negate(); })
      .apply([](auto& entry) { entry.addConstant(NTL::ZZX(1l)); });

  return mask;
}

/**
 * @brief Given a query set and a database, calculates a mask of {0,1} where 1
 * signifies a matching element and 0 otherwise.
 * @tparam TXT type of the query set. Must be a `Ptxt` or `Ctxt`.
 * @param ea The encrypted array object holding information about the scheme.
 * @param query The query set to mask against the database. Must be a row
 * vector of the same dimension as the second dimension of the database matrix.
 * @param database The matrix holding the encrypted database.
 * @return The calculated mask. Is the same size as the database.
 * @note This is an overloaded function for when the database is encrypted.
 **/
template <typename TXT>
Matrix<Ctxt> calculateMasks(const EncryptedArray& ea,
                            Matrix<TXT> query,
                            const Matrix<Ctxt>& database)
{
  if (query.dims(0) != 1)
    throw InvalidArgument("Query must be a row vector");
  if (query.dims(1) != database.dims(1))
    throw InvalidArgument(
        "Database and query must have same number of columns");
  // TODO: case where query.dims(0) != database.dims(0)

  // Replicate the query once per row of the database
  // TODO: Some such replication will be needed once blocks/bands exist
  std::vector<long> columns(database.dims(0), 0l);
  Matrix<TXT>& mask = query;
  mask.inPlaceTranspose();
  mask = mask.columns(columns);
  mask.inPlaceTranspose();

  // FIXME: Avoid deep copy
  // Ptxt Query
  if constexpr (std::is_same_v<TXT, Ptxt<BGV>>) {
    auto tmp = database.deepCopy();
    (tmp -= mask)
        .apply([&](auto& entry) { mapTo01(ea, entry); })
        .apply([](auto& entry) { entry.negate(); })
        .apply([](auto& entry) { entry.addConstant(NTL::ZZX(1l)); });

    return tmp;
  } else { // Ctxt Query
    (mask -= database)
        .apply([&](auto& entry) { mapTo01(ea, entry); })
        .apply([](auto& entry) { entry.negate(); })
        .apply([](auto& entry) { entry.addConstant(NTL::ZZX(1l)); });

    return mask;
  }
}

/**
 * @brief Given a mask and information about the query to be performed,
 * calculates a score for each matching element signified by the mask.
 * @tparam TXT type of the mask matrix. Must be a `Ptxt` or `Ctxt`.
 * @param index_sets The set of indicies signifying which columns of the mask
 * to query.
 * @param offsets The constant term to be added to the final score of each
 * queried column.
 * @param weights The weighted importance assigned to each queried column.
 * @param mask The mask with which to calculate the score from.
 * @return A single `Ctxt` or `Ptxt` containing the total score for each
 * queried column.
 **/
template <typename TXT>
inline Matrix<TXT> calculateScores(
    const std::vector<std::vector<long>> index_sets,
    const std::vector<long>& offsets,
    const std::vector<Matrix<long>>& weights,
    const Matrix<TXT>& mask)
{
  assertEq<InvalidArgument>(index_sets.size(),
                            offsets.size(),
                            "index_sets and offsets must have matching size");
  assertEq<InvalidArgument>(index_sets.size(),
                            weights.size(),
                            "index_sets and weights must have matching size");
  auto ones(mask(0, 0));
  ones.clear();
  ones.addConstant(NTL::ZZX(1L));
  Matrix<TXT> result(ones, mask.dims(0), 1l);
  for (std::size_t i = 0; i < index_sets.size(); ++i) {
    const auto& index_set = index_sets.at(i);
    const auto& weight_set = weights.at(i);
    long offset = offsets.at(i);

    assertEq<InvalidArgument>(
        weight_set.dims(0),
        index_set.size(),
        "found mismatch between index set size and weight set size");

    assertEq<InvalidArgument>(weight_set.dims(1),
                              1lu,
                              "all weight sets must be column vectors");

    Matrix<TXT> submatrix = mask.columns(index_set);
    Matrix<TXT> factor(submatrix * weight_set);
    // factor should in fact be a 1*1 matrix
    factor.apply([&](auto& entry) { entry.addConstant(NTL::ZZX(offset)); });
    result.template entrywiseOperation<TXT>(
        factor,
        [](auto& lhs, const auto& rhs) -> decltype(auto) {
          lhs.multiplyBy(rhs);
          return lhs;
        });
  }
  return result;
}

/**
 * @brief Given a value, encode the value across the coefficients of a
 * polynomial.
 * @param input The value of which to encode.
 * @param context The context object holding information on how to encode the
 * value.
 * @return A polynomial representing the encoded value.
 **/
inline PolyMod partialMatchEncode(uint32_t input, const Context& context)
{
  const long p = context.getP();
  std::vector<long> coeffs(context.getOrdP());
  // TODO - shouldn't keep checking input.
  for (long i = 0; i < long(coeffs.size()) && input != 0; ++i) {
    coeffs[i] = input % p;
    input /= p;
  }
  return PolyMod(coeffs, context.getSlotRing());
}

struct Expr;
class ColNumber;

/**
 * @brief A class wrapping a shared pointer to an `Expr` object.
 **/
class QueryExpr
{
public:
  std::shared_ptr<Expr> exp;
  /**
   * @brief Constructor.
   * @param p The `std::shared_ptr` to wrap into a `QueryExpr`.
   **/
  QueryExpr(std::shared_ptr<Expr> p) : exp(p) {}
};

/**
 * @struct Expr
 * @brief Base structure for logical expressions.
 * @note This is pure virtual.
 **/
struct Expr
{
  virtual std::string eval() const = 0;
  virtual ~Expr() = default;
};

/**
 * @class ColNumber
 * @brief An object representing a column of a database as an expression which
 * inherits from `Expr`.
 **/
class ColNumber : public Expr
{
public:
  /**
   * @brief Function for returning the column number of the object.
   * @return A string representation of the column number.
   **/
  std::string eval() const override { return std::to_string(column); }

  /**
   * @brief Constructor.
   * @param c The column number.
   **/
  ColNumber(long c) : column(c) {}

private:
  long column;
};

/**
 * @brief Utility function for creating a `QueryExpr` for a specified column
 * in a query.
 * @param cl The index of the column to be used in the query.
 * @return `QueryExpr` with the specified `ColNumber` as the `Expr`.
 **/
inline QueryExpr makeQueryExpr(long cl)
{
  ColNumber col(cl);
  return QueryExpr(std::make_shared<ColNumber>(col));
}

/**
 * @class And
 * @brief An object representing the logical `AND` expression which inherits
 *from `Expr`.
 **/
class And : public Expr
{
public:
  /**
   * @brief Function for returning the logical `AND` expression in reverse
   *polish notation where the `AND` operation is represented by `&&` and each
   *operand is a column number.
   * @return A string representing the `AND` expression in reverse polish
   * notation.
   **/
  std::string eval() const override
  {
    return lhs.exp->eval() + " " + rhs.exp->eval() + " &&";
  }

  /**
   * @brief Constructor.
   * @param l The left operand of the expression.
   * @param r The right operand of the expression.
   **/
  And(const QueryExpr& l, const QueryExpr& r) : lhs(l), rhs(r) {}

private:
  QueryExpr lhs;
  QueryExpr rhs;
};

/**
 * @class Or
 * @brief An object representing the logical OR expression which inherits from
 * `Expr`.
 **/
class Or : public Expr
{
public:
  /**
   * @brief Function for returning the logical OR expression in reverse polish
   * notation where the OR operation is represented by `||` and each operand
   * is a column number.
   * @return A string representing the OR expression in reverse polish
   * notation.
   **/
  std::string eval() const override
  {
    return lhs.exp->eval() + " " + rhs.exp->eval() + " ||";
  }

  /**
   * @brief Constructor.
   * @param l The left operand of the expression.
   * @param r The right operand of the expression.
   **/
  Or(const QueryExpr& l, const QueryExpr& r) : lhs(l), rhs(r) {}

private:
  QueryExpr lhs;
  QueryExpr rhs;
};

/**
 * @brief Overloaded operator for creating a `QueryExpr` to an `AND`
 * expression.
 * @param lhs Left operand of the `AND` expression.
 * @param rhs Right operand of the `AND` expression.
 * @return `QueryExpr` with the `AND` as the `Expr`.
 **/
inline QueryExpr operator&&(const QueryExpr& lhs, const QueryExpr& rhs)
{
  return QueryExpr(std::make_shared<And>(lhs.exp, rhs.exp));
}

/**
 * @brief Overloaded operator for creating a `QueryExpr` of an `OR`
 * expression.
 * @param lhs Left operand of the `OR` expression.
 * @param rhs Right operand of the `OR` expression.
 * @return `QueryExpr` with the OR as the `Expr`.
 **/
inline QueryExpr operator||(const QueryExpr& lhs, const QueryExpr& rhs)
{
  return QueryExpr(std::make_shared<Or>(lhs.exp, rhs.exp));
}

/**
 * @brief free copy of isNumber to use in this function
 */
bool isNumber(const std::string& s)
{
  // Positive only
  return std::all_of(s.begin(), s.end(), ::isdigit);
}

/**
 * @brief Construct a `QueryExpr` from a string
 * @param inFixQuery string of the logical query to build. Space separated,
 * operators of the form AND, OR
 * @note uses Dijkstra two stack algorithm to construct a QueryExpr
 **/
QueryExpr buildQuery(const std::string& inFixQuery)
{
  // checks for well formed query: no consecutive operators, spaces around ANDs
  // and ORs, no consecutive operands (doesn't reject e.g. "(x y")
  std::regex consecOps("(AND|OR|\\()+?\\s*(AND|OR|\\))+?");
  std::regex opSpaces("(AND|OR)+?\\S|\\S(AND|OR)+?");
  // std::regex consecArgs("\\b(?!AND|OR)\\w+\\s*\\b(?!AND|OR)\\w+");
  assertFalse(regex_search(inFixQuery.begin(), inFixQuery.end(), consecOps),
              "invalid query, found consecutive operators");
  assertFalse(regex_search(inFixQuery.begin(), inFixQuery.end(), opSpaces),
              "invalid query, require spaces around ANDs and ORs");
  // taking this out to allow for multi word arguments e.g. date of birth
  /**assertFalse(regex_search(inFixQuery.begin(), inFixQuery.end(), consecArgs),
              "invalid query, found consecutive arguments");**/

  std::stack<char> operatorStack; // stack for operators. Making this chars for
                                  // easy unpacking
  std::stack<QueryExpr> operandStack; // stack for operands

  // add spaces in the input stream before and after brackets
  std::stringstream input{" "};
  for (int i = 0; i < inFixQuery.size(); i++) {
    if (inFixQuery[i] == '(')
      input << " ( ";
    else if (inFixQuery[i] == ')')
      input << " ) ";
    else
      input << inFixQuery[i];
  }
  input << " )";
  operatorStack.push('(');
  std::string symbol;
  while (input >> symbol) {
    if (!symbol.compare("("))
      operatorStack.push('(');
    else if (!symbol.compare("AND")) {
      if (operatorStack.top() == '&') { // multiAND clause
        QueryExpr rhs = operandStack.top();
        operandStack.pop();
        QueryExpr lhs = operandStack.top();
        operandStack.pop();
        QueryExpr newop = lhs && rhs;
        operandStack.push(newop);
      } else {
        operatorStack.push('&');
      }
    } else if (!symbol.compare("OR")) {
      if (operatorStack.top() == '|') { // multiOR clause
        QueryExpr rhs = operandStack.top();
        operandStack.pop();
        QueryExpr lhs = operandStack.top();
        operandStack.pop();
        QueryExpr newop = lhs || rhs;
        operandStack.push(newop);
      } else if (operatorStack.top() == '&') { // AND has precedence
        QueryExpr rhs = operandStack.top();
        operandStack.pop();
        QueryExpr lhs = operandStack.top();
        operandStack.pop();
        QueryExpr newop = lhs && rhs;
        operandStack.push(newop);
        operatorStack.pop();
        operatorStack.push('|');
      } else {
        operatorStack.push('|');
      }
    } else if (!symbol.compare(")")) {
      while (operatorStack.top() != '(') {
        if (operatorStack.top() == '&') {
          QueryExpr rhs = operandStack.top();
          operandStack.pop();
          QueryExpr lhs = operandStack.top();
          operandStack.pop();
          QueryExpr newop = lhs && rhs;
          operandStack.push(newop);
          operatorStack.pop();
        } else if (operatorStack.top() == '|') {
          QueryExpr rhs = operandStack.top();
          operandStack.pop();
          QueryExpr lhs = operandStack.top();
          operandStack.pop();
          QueryExpr newop = lhs || rhs;
          operandStack.push(newop);
          operatorStack.pop();
        } else {
          throw InvalidArgument("unrecognised operator");
        }
      }
      operatorStack.pop(); // pop the "("
    } else {
      // Should be a number
      assertTrue(isNumber(symbol), "String is not a number: '" + symbol + "'");
      operandStack.push(makeQueryExpr(std::stoi(symbol)));
    }
  }
  assertEq<LogicError>(0UL,
                       operatorStack.size(),
                       "operatorStack should be empty");
  assertEq<LogicError>(1UL,
                       operandStack.size(),
                       "operandStack should have one element");
  return operandStack.top();
}
/**
 * @struct QueryType
 * @brief Structure containing all information required for an HE query.
 **/
struct QueryType
{
  /**
   * @brief `std::vector` of index sets. These index sets specify the indexes
   * of the columns in each column subset.
   **/
  std::vector<std::vector<long>> Fs;

  /**
   * @brief `std::vector` of offsets. Each offset is a constant value. There
   * should be a single offset for each index set.
   **/
  std::vector<long> mus;

  /**
   * @brief `std::vector` of a set of weights. Each weight set corresponds to a
   * single index set where each individual weight corresponds to the index of
   * the index set.
   **/
  std::vector<Matrix<long>> taus;

  /**
   * @brief Flag indicating if the query contains a logical OR operation. This
   * is used for optimization purposes.
   **/
  bool containsOR = false;

  /**
   * @brief Constructor.
   * @param index_sets The set of column subsets.
   * @param offsets The set of offset constants.
   * @param weights The set of weight sets.
   * @param isThereAnOR Boolean value indicating if the query contains an OR
   * operation.
   **/
  QueryType(const std::vector<std::vector<long>>& index_sets,
            const std::vector<long>& offsets,
            const std::vector<Matrix<long>>& weights,
            const bool isThereAnOR) :
      Fs(index_sets), mus(offsets), taus(weights), containsOR(isThereAnOR)
  {
  }

  /**
   * @brief Constructor.
   * @param index_sets The set of column subsets.
   * @param offsets The set of offset constants.
   * @param weights The set of weight sets.
   * @param isThereAnOR Boolean value indicating if the query contains an OR
   * operation.
   **/
  QueryType(std::vector<std::vector<long>>&& index_sets,
            std::vector<long>&& offsets,
            std::vector<Matrix<long>>&& weights,
            bool isThereAnOR) :
      Fs(index_sets), mus(offsets), taus(weights), containsOR(isThereAnOR)
  {
  }
};

/**
 * @class QueryBuilder
 * @brief An object used to construct a `QueryType` object from a logical
 * expression.
 **/
class QueryBuilder
{

  // 'outer' vec are the and groups and 'inner' are the or groups
  using vecvec = std::vector<std::vector<long>>;

public:
  /**
   * @brief Constructor.
   * @param expr The logical expression to build.
   * @note The expression is evaluated to reverse polish notation.
   **/
  QueryBuilder(const QueryExpr& expr) : query_str(expr.exp->eval()) {}

  /**
   * @brief Constructor.
   * @param expr query string to build, in infix notation.
   * @note The expression is evaluated to reverse polish notation.
   **/
  QueryBuilder(const std::string query) : query_str(convertToPostFix(query)) {}

  /**
   * @brief Function for building the `QueryType` object from the expression.
   * @param columns The total number of columns in the column set.
   * @return The resultant `QueryType` object containing information relating to
   * the query.
   **/
  QueryType build(long columns) const
  {

    // Convert the query to "type 1" by expanding out necessary ORs
    vecvec expr = expandOr(query_str);
    bool containsOR = false;

    vecvec Fs(expr.size());
    {
      std::vector<long> v(columns);
      std::iota(v.begin(), v.end(), 0);
      std::fill(Fs.begin(), Fs.end(), v);
    }
    std::vector<long> mus(expr.size(), 1);
    std::vector<Matrix<long>> taus;
    taus.reserve(expr.size());

    // Create the taus
    for (long i = 0; i < long(expr.size()); ++i) { // Each tau
      mus[i] = 0;                                  // Set mu to zero.
      Matrix<long> M(columns, 1);                  // Create temp tau matrix
      containsOR = (expr[i].size() > 1) ? true : containsOR;
      for (long j = 0; j < long(expr[i].size()); ++j) // Each column index
        M(expr[i][j], 0) = 1;                         // Mark those columns as 1
      taus.push_back(std::move(M));
    }

    return QueryType(std::move(Fs),
                     std::move(mus),
                     std::move(taus),
                     containsOR);
  }

  // function to return query_str
  std::string getQueryString() const { return query_str; }

private:
  std::string query_str;

  /**
   * @brief Function that converts a space separated infix query with ANDs, ORs,
   *and NOTs to reverse polish notation.
   * @param s Input string in infix notation.
   * @return a new string in polish notation.
   **/
  inline std::string convertToPostFix(const std::string& s)
  {
    // checks for well formed query: no consecutive operators, spaces around
    // ANDs
    // and ORs, no consecutive operands (doesn't reject e.g. "(x y")
    std::regex consecOps("(AND|OR|\\()+?\\s*(AND|OR|\\))+?");
    std::regex opSpaces("(AND|OR)+?\\S|\\S(AND|OR)+?");
    // std::regex consecArgs("\\b(?!AND|OR)\\w+\\s*\\b(?!AND|OR)\\w+");
    assertFalse(regex_search(s.begin(), s.end(), consecOps),
                "invalid query, found consecutive operators");
    assertFalse(regex_search(s.begin(), s.end(), opSpaces),
                "invalid query, require spaces around ANDs and ORs");
    // taking this out to allow for multi word arguments
    /**assertFalse(regex_search(s.begin(), s.end(), consecArgs),
                "invalid query, found consecutive arguments");**/
    std::string ret = "";
    std::stack<char> st;
    // add spaces in the input stream before and after brackets
    std::stringstream input{" "};
    for (int i = 0; i < s.size(); i++) {
      if (s[i] == '(')
        input << " ( ";
      else if (s[i] == ')')
        input << " ) ";
      else
        input << s[i];
    }
    input << " )";
    st.push('(');

    std::string symbol;
    while (input >> symbol) {
      if (!symbol.compare("(")) {
        st.push('(');
      } else if (!symbol.compare("AND") && st.top() == '&') {
        // in a multiAND clause
        ret += " &&";
      } else if (!symbol.compare("AND")) {
        // not in a multiAND clause
        st.push('&');
      } else if (!symbol.compare("OR") && st.top() == '&') {
        //&& has higher precedence than ||
        ret += " &&";
        st.pop();
        st.push('|');
      } else if (!symbol.compare("OR") && st.top() == '|') {
        // in a multiOR clause
        ret += " ||";
      } else if (!symbol.compare("OR")) {
        st.push('|');
      } else if (!symbol.compare(")")) { // empty stack until hit a "("
        while (st.top() != '(') {
          // add either " &&" or " ||"
          ret += " ";
          ret += st.top();
          ret += st.top();
          st.pop();
        }
        st.pop(); // pop the "("
      } else {    // operands
        if (ret.length() != 0)
          ret += " ";
        ret += symbol;
      }
    }

    assertEq<LogicError>(
        0UL,
        st.size(),
        "error -- after conversion stack should be empty: query badly formed");

    return ret;
  }
  /**
   * @brief Free function that converts a logical expression in infix notation
   *to reverse polish notation.
   * @param s Input string in infix notation. Can have nonbracketed clauses,
   *single operators (|,&), and multichar arguments
   * @return The string in polish notation with double operators separated by
   *spaces.
   **/
  inline std::string convertToPostFixOld(std::string s)
  {
    std::string ret = "";
    std::stack<char> st;
    st.push('('); // push open bracket onto stack
    s += ")";     // append closing bracket to string

    for (int i = 0; i < s.length(); i++) {
      char c = s[i];
      switch (c) {
      case '&':
        if (st.top() == '&') {
          ret += " &&"; // in a nonbinary And clause
          while (s[i + 1] == '&')
            i++; // skip next index as it's another &
        } else {
          st.push(c);
          while (s[i + 1] == '&')
            i++; // skip next index as it's another &
        }
        break;
      case '|':
        if (st.top() == '&') {
          ret += " &&"; //&& has higher precedence than ||
          st.pop();     // pop the &&
          st.push(c);   // push the ||
          while (s[i + 1] == '|')
            i++; // skip next index as it's another |
        } else if (st.top() == '|') {
          ret += " ||"; // in a nonbinary or clause
          while (s[i + 1] == '|')
            i++; // skip next index as it's another |
        } else {
          st.push(c);
          while (s[i + 1] == '|')
            i++; // skip next index as it's another |
        }
        break;
      case '(':
        st.push(c);
        break;
      case ')':
        while (st.top() != '(') {
          if (st.top() == '&') {
            ret += " &&";
            st.pop();
          } else if (st.top() == '|') {
            ret += " ||";
            st.pop();
          }
        }
        st.pop(); // pop open bracket '('
        break;

      case ' ':
        break;                 // not pushing spaces
      default:                 // operands
        if (ret.length() != 0) // string non empty, prepend a space
          ret += " ";
        ret += c;
        while (!((s[i + 1] == '&') || (s[i + 1] == '|') || (s[i + 1] == ')') ||
                 (s[i + 1] == ' '))) {
          // multichar operands
          ret += s[i + 1];
          i++;
        }
        break;
      }
    }

    assertEq<LogicError>(
        0UL,
        st.size(),
        "error -- after conversion stack should be empty: query badly formed");
    return ret;
  }

  void printStack(std::stack<vecvec> stack)
  {
    while (!stack.empty()) {
      printVecVec(stack.top());
      stack.pop();
    }
  }

  void printVecVec(const vecvec& vv)
  {
    for (const auto& v : vv) {
      std::cout << "[ ";
      for (const auto& e : v) {
        std::cout << e << " ";
      }
      std::cout << "]";
    }
    std::cout << "\n";
  }

  bool isNumber(const std::string& s) const
  {
    // Positive only
    return std::all_of(s.begin(), s.end(), ::isdigit);
  }

  vecvec expandOr(const std::string& s) const
  {
    std::stack<vecvec> convertStack;

    std::istringstream input{s};
    std::ostringstream output{};

    std::string symbol;

    while (input >> symbol) {
      if (!symbol.compare("&&")) {
        // Squash the top into penultimate.
        auto op = convertStack.top();
        convertStack.pop();
        auto& top = convertStack.top();
        top.insert(top.end(), op.begin(), op.end());
      } else if (!symbol.compare("||")) {
        // Cartesian-esque product
        auto op1 = convertStack.top();
        convertStack.pop();
        auto op2 = convertStack.top();
        convertStack.pop();

        vecvec prod;
        prod.reserve(op1.size() * op2.size());
        for (const auto& i : op1)
          for (const auto& j : op2) {
            auto x = i;
            x.insert(x.end(), j.begin(), j.end());
            prod.push_back(std::move(x));
          }

        convertStack.push(std::move(prod));
      } else {
        // Assume it is a number. But sanity check anyway.
        assertTrue(isNumber(symbol),
                   "String is not a number: '" + symbol + "'");
        convertStack.emplace(vecvec(1, {std::stol(symbol)}));
      }
    }

    // Now read answer off stack (should be size == 1).
    assertEq<LogicError>(1UL,
                         convertStack.size(),
                         "Size of stack after expandOr should be 1");

    return std::move(convertStack.top());
  }
};

/**
 * @brief A class for converting cleartext queries with column labels to PSI
 * queries with column numbers.
 * Column labels are stored in col_names.
 * Optional support for columns that are wider than a single ciphertext. This is
 * stored in col_dims.
 */
class Table
{
public:
  /**
   * @brief Construct a new Table object from column labels only.
   * Constructs a table which has all columns of ctxt width = 1.
   * @param cols tuple of column labels
   */

  Table(std::vector<std::string> cols) :
      col_names(cols),
      col_dims(std::vector(cols.size(), 1L)),
      col_count(cols.size())
  {
  }

  /**
   * @brief Construct a new Table object from column labels and dimensions.
   * @param cols tuple of column labels
   * @param dims tuple of column ciphertext dimensions
   */
  // Should this assert all dims > 0?
  Table(std::vector<std::string> cols, std::vector<long> dims) :
      col_names(cols), col_dims(dims)
  {
    assertEq<LogicError>(
        cols.size(),
        dims.size(),
        "Error, number of columns and number of dimensions mismatch");
    col_count = std::accumulate(dims.begin(), dims.end(), 0);
  }
  void append(std::string col, long dim = 1L)
  {
    col_names.push_back(col);
    col_dims.push_back(dim);
    col_count += dim;
  }
  /**
   * @brief insert a new column at index
   * @param index the Table index to insert the new column, using zero ordering
   * @param col the column label
   * @param dim the ciphertext dimension of the new column
   */
  void insert(long index, std::string col, long dim = 1L)
  {
    // sanitise/check index?
    col_names.insert(col_names.begin() + index, col);
    col_dims.insert(col_dims.begin() + index, dim);
    col_count += dim;
  }
  /**
   * @brief resize the width of a column
   * @param index the column to resize
   * @param newdim the new column width
   */
  void resize(long index, long newdim)
  {
    assertTrue<LogicError>(newdim > 0,
                           "error, column width must be a positive integer");
    col_count += (newdim - col_dims[index]);
    col_dims[index] = newdim;
  }
  void erase(long index)
  {
    // sanitise/check index?
    col_count -= col_dims[index];
    col_names.erase(col_names.begin() + index);
    col_dims.erase(col_dims.begin() + index);
  }
  /**
   * @brief Build a `QueryExpr` object from a table query.
   *
   * @param s A query with space separated ANDs and ORs and column labels as
   * operands.
   * @return `QueryExpr` with database column numbers as operands.
   */
  QueryExpr buildQueryExpr(const std::string& s) const
  {
    std::unordered_map<std::string, QueryExpr> operands = buildOperands();
    std::stack<char> operatorStack;     // stack for operators
    std::stack<QueryExpr> operandStack; // stack for operands
    // add spaces in the input stream before and after brackets
    std::stringstream input{" "};
    for (int i = 0; i < s.size(); i++) {
      if (s[i] == '(')
        input << " ( ";
      else if (s[i] == ')')
        input << " ) ";
      else
        input << s[i];
    }
    input << " )";
    operatorStack.push('(');
    std::string symbol;
    std::string col = "";
    while (input >> symbol) {
      if (!symbol.compare("("))
        operatorStack.push('(');
      else if (!symbol.compare("AND") || !symbol.compare("OR") ||
               !symbol.compare(")")) {
        // make an operator fpr the operator stack
        if (col.length() != 0) {
          assertTrue(operands.find(col) != operands.end(),
                     "unrecognised column: '" + col + "'");
          operandStack.push(operands.at(col));
          col = "";
        }

        if (!symbol.compare("AND") || !symbol.compare("OR")) {
          if ((operatorStack.top() == '&') ||
              ((operatorStack.top() == '|') && !symbol.compare("OR"))) {
            // cases where precedence needs to be handled
            QueryExpr rhs = operandStack.top();
            operandStack.pop();
            QueryExpr lhs = operandStack.top();
            operandStack.pop();
            operandStack.push((operatorStack.top() == '&') ? (lhs && rhs)
                                                           : (lhs || rhs));
            operatorStack.pop();
          }
          // push new operator to the stack
          operatorStack.push((!symbol.compare("AND")) ? '&' : '|');
        } else {
          // must be a ')': flush the operator stack
          while (operatorStack.top() != '(') {
            QueryExpr rhs = operandStack.top();
            operandStack.pop();
            QueryExpr lhs = operandStack.top();
            operandStack.pop();
            operandStack.push((operatorStack.top() == '&') ? (lhs && rhs)
                                                           : (lhs || rhs));
            operatorStack.pop();
          }
          operatorStack.pop(); // pop the "("
        }
      } else {
        col += (col.length() == 0) ? symbol : " " + symbol;
      }
    }
    assertEq<LogicError>(0UL,
                         operatorStack.size(),
                         "operatorStack should be empty");
    assertEq<LogicError>(1UL,
                         operandStack.size(),
                         "operandStack should have one element");
    return operandStack.top();
  }

  QueryBuilder buildQueryBuilder(const std::string& s) const
  {
    std::unordered_map<std::string, std::string> operands =
        buildOperandStrings();
    std::string scopy = s;
    for (std::string col : col_names) {
      scopy = std::regex_replace(scopy, std::regex(col), operands[col]);
    }
    return QueryBuilder(scopy);
  }

private:
  std::vector<std::string> col_names;
  std::vector<long> col_dims;
  long col_count;
  std::unordered_map<std::string, QueryExpr> buildOperands() const
  {
    long current_index = 0;
    std::unordered_map<std::string, QueryExpr> operands;
    for (int i = 0; i < col_names.size(); i++) {
      // assert dim > 0?
      QueryExpr op = makeQueryExpr(current_index);
      current_index++;
      for (int j = 1; j < col_dims[i]; j++) {
        QueryExpr newop = op && makeQueryExpr(current_index);
        op = newop;
        current_index++;
      }
      operands.insert(std::make_pair(col_names[i], op));
    }
    return operands;
  }

  std::unordered_map<std::string, std::string> buildOperandStrings() const
  {
    long current_index = 0;
    std::unordered_map<std::string, std::string> operands;
    for (int i = 0; i < col_names.size(); i++) {
      // assert dim > 0?
      std::string op = std::to_string(current_index);
      current_index++;
      for (int j = 1; j < col_dims[i]; j++) {
        op += " AND " + std::to_string(current_index);
        current_index++;
      }
      operands[col_names[i]] = op;
    }
    return operands;
  }
};

/**
 * @brief free function taking a table description of the form
 * Table(col1, col2, col3,...) and creates a Table object with
 * these column labels.
 *
 * @param s a string of the form Table( * , * , * , ...), with no commas
 * in column labels.
 * @return Table with labels as column labels and every ctxt dimension equal
 * to 1.
 */
Table makeTableFromString(const std::string& s)
{
  // check string is of the form Table()
  std::string prefix = "Table(";
  assertTrue<LogicError>(s.rfind(prefix, 0) == 0,
                         "string does not start with Table key word");
  assertTrue<LogicError>(s[s.length() - 1] == ')',
                         "string does not end with )");
  // construct a stringstream to parse input. Need spaces before and after
  // commas and skip final bracket
  std::stringstream input{" "};
  for (int i = prefix.length(); i < s.length() - 1; i++) {
    if (s[i] == ',')
      input << " , ";
    else {
      input << s[i];
    }
  }
  std::vector<std::string> columns;
  std::string symbol;
  std::string col = "";
  while (input >> symbol) {
    if (!symbol.compare(",")) {
      columns.push_back(col);
      col = "";
    } else {
      col += (col.length() == 0) ? symbol : " " + symbol;
    }
  }
  columns.push_back(col);
  return Table(columns);
}
/**
 * @class Database
 * @tparam TXT The database is templated on `TXT` which can either be a `Ctxt`
 * or a `Ptxt<BGV>`
 * @brief An object representing a database which is a `HElib::Matrix<TXT>`.
 **/
template <typename TXT>
class Database
{
public:
  // FIXME: Generally, should Database own the Matrix uniquely?
  // Should we force good practice and ask that Context always be shared_ptr?

  // FIXME: Should probably move Matrix or make it unique_ptr or both?
  /**
   * @brief Constructor.
   * @param M The `Matrix<TXT>` containing the data of the database.
   * @param c A shared pointer to the context used to create the data.
   **/
  Database(const Matrix<TXT>& M, std::shared_ptr<const Context> c) :
      data(M), context(c)
  {
  }

  // FIXME: Should this option really exist?
  /**
   * @brief Constructor.
   * @param M The `Matrix<TXT>` containing the data of the database.
   * @param c The context object used to create the data.
   * @note This version accepts a `Context` that this object is not responsible
   * for i.e. if it is on the stack. The programmer is responsible in this case
   * for scope.
   **/
  Database(const Matrix<TXT>& M, const Context& c) :
      data(M),
      context(std::shared_ptr<const helib::Context>(&c, [](auto UNUSED p) {}))
  {
  }

  // FIXME: Combination of TXT = ctxt and TXT2 = ptxt does not work
  /**
   * @brief Function for performing a database lookup given a query expression
   * and query data.
   * @tparam TXT2 The type of the query data, can be either a `Ctxt` or
   * `Ptxt<BGV>`.
   * @param lookup_query The lookup query expression to perform.
   * @param query_data The lookup query data to compare with the database.
   * @return A `Matrix<TXT2>` containing 1s and 0s in slots where there was a
   * match or no match respectively.
   **/
  template <typename TXT2>
  auto contains(const QueryType& lookup_query,
                const Matrix<TXT2>& query_data) const;

  // FIXME: Combination of TXT = ctxt and TXT2 = ptxt does not work
  /**
   * @brief Function for performing a weighted partial match given a query
   * expression and query data.
   * @tparam TXT2 The type of the query data, can be either a `Ctxt` or
   * `Ptxt<BGV>`.
   * @param weighted_query The weighted lookup query expression to perform.
   * @param query_data The query data to compare with the database.
   * @return A `Matrix<TXT2>` containing a score on weighted matches.
   **/
  template <typename TXT2>
  auto getScore(const QueryType& weighted_query,
                const Matrix<TXT2>& query_data) const;

  // TODO - correct name?
  /**
   * @brief Returns number of columns in the database.
   * @return The number of columns in the database.
   **/
  long columns() { return data.dims(1); }

  Matrix<TXT>& getData();

private:
  Matrix<TXT> data;
  std::shared_ptr<const Context> context;
};

template <typename TXT>
template <typename TXT2>
inline auto Database<TXT>::contains(const QueryType& lookup_query,
                                    const Matrix<TXT2>& query_data) const
{
  auto result = getScore<TXT2>(lookup_query, query_data);

  if (lookup_query.containsOR) {
    // FLT on the scores
    result.apply([&](auto& txt) {
      txt.power(context->getAlMod().getPPowR() - 1);
      return txt;
    });
  }

  return result;
}

template <typename TXT>
template <typename TXT2>
inline auto Database<TXT>::getScore(const QueryType& weighted_query,
                                    const Matrix<TXT2>& query_data) const
{
  auto mask = calculateMasks(context->getEA(), query_data, this->data);

  auto result = calculateScores(weighted_query.Fs,
                                weighted_query.mus,
                                weighted_query.taus,
                                mask);

  return result;
}

template <typename TXT>
inline Matrix<TXT>& Database<TXT>::getData()
{
  return data;
}

} // namespace helib

#endif
