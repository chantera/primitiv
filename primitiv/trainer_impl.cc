#include <config.h>

#include <algorithm>
#include <cmath>
#include <primitiv/parameter.h>
#include <primitiv/tensor_ops.h>
#include <primitiv/trainer_impl.h>

namespace primitiv {
namespace trainers {

void SGD::configure_parameter(Parameter &param) {}

void SGD::update_parameter(float scale, Parameter &param) {
  param.value() -= (scale * eta_) * param.gradient();
}

void SGD::get_configs(
    std::unordered_map<std::string, unsigned> &uint_configs,
    std::unordered_map<std::string, float> &float_configs) const {
  float_configs.insert(std::make_pair("SGD.eta", eta_));
}

void SGD::set_configs(
    const std::unordered_map<std::string, unsigned> &uint_configs,
    const std::unordered_map<std::string, float> &float_configs) {
  eta_ = float_configs.at("SGD.eta");
}

void Adam::configure_parameter(Parameter &param) {
  for (const char *name : {"adam-m1", "adam-m2"}) {
    if (!param.has_stats(name)) {
      param.add_stats(name, param.shape());
      param.stats(name).reset(0);
    }
  }
}

void Adam::update_parameter(float scale, Parameter &param) {
  const unsigned epoch = get_epoch() + 1;
  const Tensor &g = param.gradient();
  Tensor &m1 = param.stats("adam-m1");
  Tensor &m2 = param.stats("adam-m2");
  m1 = beta1_ * m1 + (1 - beta1_) * g;
  m2 = beta2_ * m2 + (1 - beta2_) * g * g;
  const Tensor mm1 = m1 / (1 - std::pow(beta1_, epoch));
  const Tensor mm2 = m2 / (1 - std::pow(beta2_, epoch));
  param.value() -= (scale * alpha_) * mm1 / (tensor_ops::sqrt(mm2) + eps_);
}

void Adam::get_configs(
    std::unordered_map<std::string, unsigned> &uint_configs,
    std::unordered_map<std::string, float> &float_configs) const {
  float_configs.insert(std::make_pair("Adam.alpha", alpha_));
  float_configs.insert(std::make_pair("Adam.beta1", beta1_));
  float_configs.insert(std::make_pair("Adam.beta2", beta2_));
  float_configs.insert(std::make_pair("Adam.eps", eps_));
}

void Adam::set_configs(
    const std::unordered_map<std::string, unsigned> &uint_configs,
    const std::unordered_map<std::string, float> &float_configs) {
  alpha_ = float_configs.at("Adam.alpha");
  beta1_ = float_configs.at("Adam.beta1");
  beta2_ = float_configs.at("Adam.beta2");
  eps_ = float_configs.at("Adam.eps");
}

}  // namespace trainers
}  // namespace primitiv
