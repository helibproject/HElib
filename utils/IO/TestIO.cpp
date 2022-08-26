/* Copyright (C) 2022 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <helib/helib.h>
#include <helib/partialMatch.h>

#include "io.h"

namespace {

using Ptxt = helib::Ptxt<helib::BGV>;

TEST(TestIO, readingPtxtDBWorksCorrectly) {
  // clang-format off
  helib::Context context = helib::ContextBuilder<helib::BGV>()
                               .m(24)
                               .p(37)
                               .r(1)
                               .bits(100)
                               .build();
  // clang-format on
  helib::SecKey sk(context);
  sk.GenSecKey();
  addSome1DMatrices(sk);
  helib::PubKey& pk = sk;

  std::vector<long> d1 = {1, 2, 3, 4};
  std::vector<long> d2 = {5, 6, 7, 8};
  std::vector<long> d3 = {9, 10, 11, 12};
  std::vector<long> d4 = {13, 14, 15, 16};

  Ptxt p1(context, d1);
  Ptxt p2(context, d2);
  Ptxt p3(context, d3);
  Ptxt p4(context, d4);
  Ptxt zero_ptxt(context);
  helib::Matrix<Ptxt> ptxt_db(zero_ptxt, 2, 2);
  ptxt_db(0, 0) = p1;
  ptxt_db(1, 0) = p2;
  ptxt_db(0, 1) = p3;
  ptxt_db(1, 1) = p4;

  std::string ptxtFile = "db.ptxt";
  writeResultsToFile(ptxtFile, ptxt_db);

  // FIXME: This is just to allow us to call readDbFromFile
  std::stringstream ss;
  ss << context;
  std::shared_ptr<helib::Context> contextp(helib::Context::readPtrFromJSON(ss));

  auto deserialized_db = readDbFromFile<Ptxt>(ptxtFile, contextp, pk);
  auto ptxt_matrix = deserialized_db.getData();
  std::remove(ptxtFile.c_str());

  for (std::size_t i = 0; i < ptxt_matrix.dims(0); ++i) {
    for (std::size_t j = 0; j < ptxt_matrix.dims(1); ++j) {
      EXPECT_EQ(ptxt_db(i, j), ptxt_matrix(i, j));
    }
  }
}

TEST(TestIO, readingCtxtDBWorksCorrectly) {
  // clang-format off
  helib::Context context = helib::ContextBuilder<helib::BGV>()
                               .m(24)
                               .p(37)
                               .r(1)
                               .bits(100)
                               .build();
  // clang-format on
  helib::SecKey sk(context);
  sk.GenSecKey();
  addSome1DMatrices(sk);
  helib::PubKey& pk = sk;

  std::vector<long> d1 = {1, 2, 3, 4};
  std::vector<long> d2 = {5, 6, 7, 8};
  std::vector<long> d3 = {9, 10, 11, 12};
  std::vector<long> d4 = {13, 14, 15, 16};

  Ptxt p1(context, d1);
  Ptxt p2(context, d2);
  Ptxt p3(context, d3);
  Ptxt p4(context, d4);
  Ptxt zero_ptxt(context);
  helib::Matrix<Ptxt> ptxt_db(zero_ptxt, 2, 2);
  ptxt_db(0, 0) = p1;
  ptxt_db(1, 0) = p2;
  ptxt_db(0, 1) = p3;
  ptxt_db(1, 1) = p4;

  helib::Ctxt c1(pk);
  helib::Ctxt c2(pk);
  helib::Ctxt c3(pk);
  helib::Ctxt c4(pk);

  pk.Encrypt(c1, p1);
  pk.Encrypt(c2, p2);
  pk.Encrypt(c3, p3);
  pk.Encrypt(c4, p4);

  std::string ctxtFile = "db.ctxt";
  helib::Ctxt zero_ctxt(pk);
  helib::Matrix<helib::Ctxt> encrypted_db(zero_ctxt, 2, 2);
  encrypted_db(0, 0) = c1;
  encrypted_db(1, 0) = c2;
  encrypted_db(0, 1) = c3;
  encrypted_db(1, 1) = c4;
  writeResultsToFile(ctxtFile, encrypted_db);

  // FIXME: This is just to allow us to call readDbFromFile
  std::stringstream ss;
  ss << context;
  std::shared_ptr<helib::Context> contextp(helib::Context::readPtrFromJSON(ss));

  auto deserialized_db = readDbFromFile<helib::Ctxt>(ctxtFile, contextp, pk);
  auto encrypted_matrix = deserialized_db.getData();
  std::remove(ctxtFile.c_str());  // Remove file

  for (std::size_t i = 0; i < encrypted_db.dims(0); ++i) {
    for (std::size_t j = 0; j < encrypted_db.dims(1); ++j) {
      Ptxt decrypted_entry(context);
      sk.Decrypt(decrypted_entry, encrypted_matrix(i, j));
      EXPECT_EQ(ptxt_db(i, j), decrypted_entry);
    }
  }
}

TEST(TestIO, ctxtDBDeserializationAgreesWithPtxtDeserialization) {
  // clang-format off
  helib::Context context = helib::ContextBuilder<helib::BGV>()
                               .m(24)
                               .p(37)
                               .r(1)
                               .bits(100)
                               .build();
  // clang-format on
  helib::SecKey sk(context);
  sk.GenSecKey();
  addSome1DMatrices(sk);
  helib::PubKey& pk = sk;

  std::vector<long> d1 = {1, 2, 3, 4};
  std::vector<long> d2 = {5, 6, 7, 8};
  std::vector<long> d3 = {9, 10, 11, 12};
  std::vector<long> d4 = {13, 14, 15, 16};

  Ptxt p1(context, d1);
  Ptxt p2(context, d2);
  Ptxt p3(context, d3);
  Ptxt p4(context, d4);
  Ptxt zero_ptxt(context);
  helib::Matrix<Ptxt> ptxt_db(zero_ptxt, 2, 2);
  ptxt_db(0, 0) = p1;
  ptxt_db(1, 0) = p2;
  ptxt_db(0, 1) = p3;
  ptxt_db(1, 1) = p4;

  helib::Ctxt c1(pk);
  helib::Ctxt c2(pk);
  helib::Ctxt c3(pk);
  helib::Ctxt c4(pk);

  pk.Encrypt(c1, p1);
  pk.Encrypt(c2, p2);
  pk.Encrypt(c3, p3);
  pk.Encrypt(c4, p4);

  helib::Ctxt zero_ctxt(pk);
  helib::Matrix<helib::Ctxt> encrypted_db(zero_ctxt, 2, 2);
  encrypted_db(0, 0) = c1;
  encrypted_db(1, 0) = c2;
  encrypted_db(0, 1) = c3;
  encrypted_db(1, 1) = c4;

  std::string ctxtFile = "db.ctxt";
  writeResultsToFile(ctxtFile, encrypted_db);

  std::string ptxtFile = "db.ptxt";
  writeResultsToFile(ptxtFile, ptxt_db);

  // FIXME: This is just to allow us to call readDbFromFile
  std::stringstream ss;
  ss << context;
  std::shared_ptr<helib::Context> contextp(helib::Context::readPtrFromJSON(ss));

  auto deserialized_ctxt_db =
      readDbFromFile<helib::Ctxt>(ctxtFile, contextp, pk);
  auto encrypted_matrix = deserialized_ctxt_db.getData();
  std::remove(ctxtFile.c_str());  // Remove file

  auto deserialized_ptxt_db = readDbFromFile<Ptxt>(ptxtFile, contextp, pk);
  auto ptxt_matrix = deserialized_ptxt_db.getData();
  std::remove(ptxtFile.c_str());  // Remove file

  for (std::size_t i = 0; i < encrypted_db.dims(0); ++i) {
    for (std::size_t j = 0; j < encrypted_db.dims(1); ++j) {
      Ptxt decrypted_entry(context);
      sk.Decrypt(decrypted_entry, encrypted_matrix(i, j));
      EXPECT_EQ(ptxt_matrix(i, j), decrypted_entry);
    }
  }
}

}  // namespace
