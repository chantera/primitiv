#ifndef PRIMITIV_TENSOR_OPS_H_
#define PRIMITIV_TENSOR_OPS_H_

#include <primitiv/device.h>
#include <primitiv/error.h>
#include <primitiv/tensor.h>

namespace primitiv {

Tensor operator+(const Tensor &x);
Tensor operator-(const Tensor &x);
Tensor operator+(const Tensor &x, float k);
Tensor operator+(float k, const Tensor &x);
Tensor operator+(const Tensor &a, const Tensor &b);
Tensor operator-(const Tensor &x, float k);
Tensor operator-(float k, const Tensor &x);
Tensor operator-(const Tensor &a, const Tensor &b);
Tensor operator*(const Tensor &x, float k);
Tensor operator*(float k, const Tensor &x);
Tensor operator*(const Tensor &a, const Tensor &b);
Tensor operator/(const Tensor &x, float k);
Tensor operator/(float k, const Tensor &x);
Tensor operator/(const Tensor &a, const Tensor &b);

namespace tensor_ops {

Tensor slice(const Tensor &x, unsigned dim, unsigned lower, unsigned upper);
Tensor concat(const std::vector<const Tensor *> &xs, unsigned dim);

Tensor transpose(const Tensor &x);
Tensor dot(const Tensor &a, const Tensor &b);
Tensor exp(const Tensor &x);
Tensor tanh(const Tensor &x);
Tensor sigmoid(const Tensor &x);
Tensor step(const Tensor &x);
Tensor relu(const Tensor &x);

Tensor sum(const Tensor &x, unsigned dim);
Tensor logsumexp(const Tensor &x, unsigned dim);
Tensor log_softmax(const Tensor &x, unsigned dim);
Tensor softmax(const Tensor &x, unsigned dim);
Tensor broadcast(const Tensor &x, unsigned dim, unsigned size);

Tensor batch_sum(const Tensor &x);

}  // namespace tensor_ops
}  // namespace primitiv

#endif  // PRIMITIV_TENSOR_OPS_H_
