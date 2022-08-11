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

/* Copyright (C) 2022 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * TODO: Add the specific contributions
 */

#ifndef HELIB_QUERY_H
#define HELIB_QUERY_H

#include <helib/Matrix.h>

#include <stack>
#include <regex>

/**
 * @file query.h
 * @brief Implementation of classes and functions related to HE queries.
 * @note This code is to be considered experimental and APIs can be expected to
 * change without prior warning.
 **/

namespace helib {

// Forward declarations
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
  return QueryExpr(std::make_shared<ColNumber>(ColNumber(cl)));
}

/**
 * @class And
 * @brief An object representing the logical `AND` expression which inherits
 * from `Expr`.
 **/
class And : public Expr
{
public:
  /**
   * @brief Function for returning the logical `AND` expression in reverse
   * polish notation where the `AND` operation is represented by `&&` and each
   * operand is a column number.
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
 * @brief Check if string is a number. i.e. All characters are digits.
 * @param s The input string to be checked.
 * @return bool `True` if all chars are digits, `False` otherwise.
 */
inline bool isNumber(const std::string& s)
{
  // Positive only
  return std::all_of(s.begin(), s.end(), ::isdigit);
}

/**
 * @brief Construct a `QueryExpr` from a string using Dijkstra's two stack
 * algorithm.
 * @param inFixQuery string of the logical query to build. Space separated with
 * operators of the form AND, OR.
 * @return A `QueryExpr` object of the string `inFixQuery` in polish notation.
 * @note This free function is an alternative to the constructor of
 * `QueryBuilder` that accepts a string input.
 **/
inline QueryExpr buildQuery(const std::string& inFixQuery)
{
  // checks for well formed query: no consecutive operators, spaces around ANDs
  // and ORs, no consecutive operands (doesn't reject e.g. "(x y")
  std::regex consecOps("(AND|OR|\\()+?\\s*(AND|OR|\\))+?");
  std::regex opSpaces("(AND|OR)+?\\S|\\S(AND|OR)+?");

  assertFalse(regex_search(inFixQuery.begin(), inFixQuery.end(), consecOps),
              "invalid query, found consecutive operators");
  assertFalse(regex_search(inFixQuery.begin(), inFixQuery.end(), opSpaces),
              "invalid query, require spaces around ANDs and ORs");

  std::stack<char> operatorStack;     // stack for operators
  std::stack<QueryExpr> operandStack; // stack for operands

  // add spaces in the input stream before and after brackets
  std::stringstream input{" "};
  // TODO
  for (const auto& c : inFixQuery) {
    if (c == '(')
      input << " ( ";
    else if (c == ')')
      input << " ) ";
    else
      input << c;
  }
  input << " )";
  operatorStack.push('(');
  std::string symbol;
  while (input >> symbol) {
    if (symbol == "(")
      operatorStack.push('(');
    else if (symbol == "AND" || symbol == "OR") {
      if ((operatorStack.top() == '&') ||
          ((operatorStack.top() == '|') && symbol == "OR")) {
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
      operatorStack.push((symbol == "AND") ? '&' : '|');
    } else if (symbol == ")") {
      // flush the operator stack
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
    } else {               // Should be a number
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
  {}

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
  {}
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
   * @param expr Query string to build, in infix notation.
   * @note The expression is evaluated to reverse polish notation.
   **/
  QueryBuilder(const std::string& query) : query_str(convertToPostFix(query)) {}

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

  /**
   * @brief Getter function that returns the `query_str`.
   * @return The string representation of the query.
   **/
  std::string getQueryString() const { return query_str; }

private:
  std::string query_str;

  // Utility function for adding spaces to an input stream before and after
  // brackets given a query string.
  void addSpacesAroundBrackets(std::stringstream& ss, const std::string& s)
  {
    for (const auto& c : s) {
      if (c == '(')
        ss << " ( ";
      else if (c == ')')
        ss << " ) ";
      else
        ss << c;
    }
    ss << " )";
  }

  /**
   * @brief Function that converts a space separated infix query with ANDs and
   * ORs to reverse polish notation.
   * @param s Input string in infix notation.
   * @return A new string in polish notation.
   **/
  inline std::string convertToPostFix(const std::string& s)
  {
    // checks for well formed query: no consecutive operators, spaces around
    // ANDs and ORs, no consecutive operands (doesn't reject e.g. "(x y")
    std::regex consecOps("(AND|OR|\\()+?\\s*(AND|OR|\\))+?");
    std::regex opSpaces("(AND|OR)+?\\S|\\S(AND|OR)+?");
    // TODO
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

    // Add spaces in the input stream before and after brackets
    std::stringstream input{" "};
    addSpacesAroundBrackets(input, s);
    st.push('(');

    std::string symbol;
    while (input >> symbol) {
      if (symbol == "(") {
        st.push('(');
      } else if (symbol == "AND" && st.top() == '&') {
        // in a multiAND clause
        ret += " &&";
      } else if (symbol == "AND") {
        // not in a multiAND clause
        st.push('&');
      } else if (symbol == "OR" && st.top() == '&') {
        //&& has higher precedence than ||
        ret += " &&";
        st.pop();
        st.push('|');
      } else if (symbol == "OR" && st.top() == '|') {
        // in a multiOR clause
        ret += " ||";
      } else if (symbol == "OR") {
        st.push('|');
      } else if (symbol == ")") { // empty stack until hit a "("
        while (st.top() != '(') {
          // add either " &&" or " ||"
          // Note: For some reason this doesn't work with +=
          ret = ret + " " + st.top() + st.top();
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
   * to reverse polish notation.
   * @param s Input string in infix notation. Can have non-bracketed clauses,
   * single operators (|,&), and multi-character arguments
   * @return The string in polish notation with double operators separated by
   * spaces.
   **/
  // TODO: Delete this function
  inline std::string convertToPostFixOld(std::string s)
  {
    std::string ret = "";
    std::stack<char> st;
    st.push('('); // push open bracket onto stack
    s += ")";     // append closing bracket to string

    for (long i = 0; i < long(s.length()); i++) {
      char c = s[i];
      switch (c) {
      case '&':
        if (st.top() == '&') {
          ret += " &&"; // in a non-binary And clause
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

  vecvec expandOr(const std::string& s) const
  {
    std::stack<vecvec> convertStack;

    std::istringstream input{s};
    std::ostringstream output{};

    std::string symbol;

    while (input >> symbol) {
      if (symbol == "&&") {
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
 * @class Table
 * @brief A class for converting cleartext queries with column labels to PSI
 * queries with column numbers.
 * Column labels are stored in `col_names`.
 * Optional support for columns that are wider than a single ciphertext. This is
 * stored in `col_dims`.
 */
class Table
{
public:
  /**
   * @brief Construct a new `Table` object from column labels only.
   * Constructs a table which has all columns of ctxt width = 1.
   * @param cols Tuple of column labels.
   */
  Table(const std::vector<std::string>& cols) :
      col_names(cols),
      col_dims(std::vector(cols.size(), 1L)),
      col_count(cols.size())
  {}

  /**
   * @brief Construct a new `Table` object from column labels and dimensions.
   * @param cols tuple of column labels.
   * @param dims tuple of column ciphertext dimensions.
   */
  // Should this assert all dims > 0? TODO: Probably
  Table(const std::vector<std::string>& cols, const std::vector<long>& dims) :
      col_names(cols), col_dims(dims)
  {
    assertEq<LogicError>(
        cols.size(),
        dims.size(),
        "Error, number of columns and number of dimensions mismatch");
    col_count = std::accumulate(dims.begin(), dims.end(), 0);
  }

  /**
   * @brief Append a new column to the end of the `Table`.
   * @param col The column label.
   * @param dim The ciphertext dimension of the new column. Default = 1.
   */
  void append(const std::string& col, long dim = 1L)
  {
    col_names.push_back(col);
    col_dims.push_back(dim);
    col_count += dim;
  }

  /**
   * @brief Insert a new column at index.
   * @param index The `Table` index to insert the new column, using zero
   * ordering.
   * @param col The column label.
   * @param dim The ciphertext dimension of the new column. Default = 1.
   */
  void insert(long index, const std::string& col, long dim = 1L)
  {
    // sanitize/check index?
    col_names.insert(col_names.begin() + index, col);
    col_dims.insert(col_dims.begin() + index, dim);
    col_count += dim;
  }

  /**
   * @brief Resize the width of a column.
   * @param index The column to resize.
   * @param newdim The new column width.
   */
  void resize(long index, long newdim)
  {
    assertTrue<LogicError>(newdim > 0,
                           "error, column width must be a positive integer");
    col_count += (newdim - col_dims[index]);
    col_dims[index] = newdim;
  }

  /**
   * @brief Erase a column at specified index.
   * @param index The `Table` index to erase.
   */
  void erase(long index)
  {
    // sanitize/check index?
    col_count -= col_dims[index];
    col_names.erase(col_names.begin() + index);
    col_dims.erase(col_dims.begin() + index);
  }

  /**
   * @brief Build a `QueryExpr` object from a table query.
   * @param s A query with space separated ANDs and ORs and column labels as
   * operands.
   * @return A `QueryExpr` object with database column numbers as operands.
   */
  QueryExpr buildQueryExpr(const std::string& s) const
  {
    std::unordered_map<std::string, QueryExpr> operands = buildOperands();
    std::stack<char> operatorStack;     // stack for operators
    std::stack<QueryExpr> operandStack; // stack for operands
    // add spaces in the input stream before and after brackets
    std::stringstream input{" "};
    // TODO: Code duplication
    for (const auto& c : s) {
      if (c == '(')
        input << " ( ";
      else if (c == ')')
        input << " ) ";
      else
        input << c;
    }
    input << " )";
    operatorStack.push('(');
    std::string symbol;
    std::string col = "";
    while (input >> symbol) {
      if (symbol == "(")
        operatorStack.push('(');
      else if (symbol == "AND" || symbol == "OR" || symbol == ")") {
        // make an operator for the operator stack
        if (col.length() != 0) {
          assertTrue(operands.find(col) != operands.end(),
                     "unrecognized column: '" + col + "'");
          operandStack.push(operands.at(col));
          col.clear();
        }

        if (symbol == "AND" || symbol == "OR") {
          if ((operatorStack.top() == '&') ||
              ((operatorStack.top() == '|') && symbol == "OR")) {
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
          operatorStack.push((symbol == "AND") ? '&' : '|');
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

  // TODO Is this really necessary? Rethink this function
  /**
   * @brief
   * @param s
   * @return A `QueryBuilder` object...
   */
  QueryBuilder buildQueryBuilder(const std::string& s) const
  {
    std::unordered_map<std::string, std::string> operands =
        buildOperandStrings();
    std::string scopy = s;
    for (const auto& col : col_names) {
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
    for (long i = 0; i < long(col_names.size()); i++) {
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
    for (long i = 0; i < long(col_names.size()); i++) {
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
 * @brief Free function taking a table description of the form
 * `Table(col1, col2, col3,...)` and creates a `Table` object with
 * these column labels.
 *
 * @param s A string of the form `Table( * , * , * , ...)`, with no commas
 * in column labels.
 * @return A `Table` object with labels as column labels and every ctxt
 * dimension equal to 1.
 */
inline Table makeTableFromString(const std::string& s)
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
  for (long i = prefix.length(); i < long(s.length()) - 1; ++i) {
    // TODO
    // count = s.length() - prefix.length() - 1
    // string.substr(start, count);
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
    if (symbol == ",") {
      columns.push_back(col);
      col.clear();
    } else {
      col += (col.length() == 0) ? symbol : " " + symbol;
    }
  }
  columns.push_back(col);
  return Table(columns);
}

} // namespace helib

#endif // HELIB_QUERY_H
