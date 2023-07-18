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

#ifndef READER_H
#define READER_H

#include <vector>
#include <string>
#include <fstream>
#include <exception>

#include "stream_dispenser.h"
#include "TOC.h"

template <typename D>
class Reader
{

private:
  const std::string filepath;
  std::unique_ptr<std::istream> streamPtr;
  D& scratch;
  std::shared_ptr<TOC> toc;

public:
  Reader(const std::string& fname, D& init) :
      filepath(fname),
      streamPtr(std::make_unique<std::ifstream>(filepath, std::ios::binary)),
      scratch(init),
      toc(std::make_shared<TOC>())
  {
    const std::ifstream& stream = *reinterpret_cast<std::ifstream*>(streamPtr.get());
    if (!stream.is_open())
      throw std::runtime_error("Could not open '" + filepath + "'.");
    toc->read(*streamPtr);
  }

  template <typename STREAM>
  Reader(std::unique_ptr<STREAM>& istreamPtr, D& init) :
      filepath("__STREAM__"),
      streamPtr(std::move(istreamPtr)),
      scratch(init),
      toc(std::make_shared<TOC>())
  {
    toc->read(*streamPtr);
  }

  Reader(const Reader& rdr) :
      filepath(rdr.filepath),
      // This allows "copying" of streams by creating unique instances of the same stream
      streamPtr(make_stream_dispenser<std::ifstream>(filepath, std::ios::binary).get()),
      scratch(rdr.scratch),
      toc(rdr.toc)
  {
    const std::ifstream& stream = *reinterpret_cast<std::ifstream*>(streamPtr.get());
    if (!stream.is_open())
      throw std::runtime_error("Could not open '" + rdr.filepath + "'.");
  }

  void readDatum(D& dest, int i, int j)
  {
    if (streamPtr->eof())
      streamPtr->clear();

    streamPtr->seekg(toc->getIdx(i, j));
    dest.read(*streamPtr);
  }

  std::unique_ptr<D> readDatum(int i, int j)
  {
    if (streamPtr->eof())
      streamPtr->clear();

    std::unique_ptr<D> ptr = std::make_unique<D>(scratch);
    streamPtr->seekg(toc->getIdx(i, j));
    ptr->read(*streamPtr);

    return std::move(ptr);
  }

  std::unique_ptr<std::vector<std::vector<D>>> readAll()
  {
    if (streamPtr->eof())
      streamPtr->clear();

    auto m_ptr = std::make_unique<std::vector<std::vector<D>>>(
        toc->getRows(),
        std::vector<D>(toc->getCols(), scratch));

    for (int i = 0; i < toc->getRows(); i++) {
      for (int j = 0; j < toc->getCols(); j++) {
        streamPtr->seekg(toc->getIdx(i, j));
        (*m_ptr)[i][j].read(*streamPtr);
      }
    }

    return std::move(m_ptr);
  }

  std::unique_ptr<std::vector<D>> readRow(int i)
  {

    if (streamPtr->eof())
      streamPtr->clear();

    auto v_ptr = std::make_unique<std::vector<D>>(toc->getCols(), scratch);
    for (int n = 0; n < toc->getCols(); n++) {
      streamPtr->seekg(toc->getIdx(i, n));
      (*v_ptr)[n].read(*streamPtr);
    }

    return std::move(v_ptr);
  }

  std::unique_ptr<std::vector<D>> readCol(int j)
  {
    if (streamPtr->eof())
      streamPtr->clear();

    auto v_ptr = std::make_unique<std::vector<D>>(toc->getRows(), scratch);
    for (int n = 0; n < toc->getRows(); n++) {
      streamPtr->seekg(toc->getIdx(n, j));
      (*v_ptr)[n].read(*streamPtr);
    }

    return std::move(v_ptr);
  }

  const TOC& getTOC() const { return *toc; }
};

template <typename D>
static std::vector<Reader<D>> createReaders(long count,
                                            const std::string& dataFilePath,
                                            D& dummy)
{
  std::vector<Reader<D>> readers;
  readers.reserve(count);
  for (long i = 0; i < count; ++i) {
    readers.emplace_back(dataFilePath.c_str(), dummy);
  }
  return readers;
}

#endif // READER_H
