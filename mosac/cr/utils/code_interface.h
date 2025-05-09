#pragma once

namespace mosac::code {
// Linear code interface
class LinearCodeInterface {
 public:
  LinearCodeInterface(const LinearCodeInterface &) = delete;
  LinearCodeInterface &operator=(const LinearCodeInterface &) = delete;
  LinearCodeInterface() = default;
  virtual ~LinearCodeInterface() = default;

  // Get the dimention / length
  virtual uint32_t GetDimention() const = 0;
  virtual uint32_t GetLength() const = 0;
};

}  // namespace mosac::code
