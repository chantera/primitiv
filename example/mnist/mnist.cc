// Sample code to train/test the MNIST dataset:
//   http://yann.lecun.com/exdb/mnist/
//
// The model consists of a full-connected 2-layer (input/hidden/output)
// perceptron with the softmax cross entropy loss.
//
// Usage:
//   Run 'download_mnist.sh' in the same directory before using this code.
// g++
//   -std=c++11
//   -I/path/to/primitiv/includes (typically -I../..)
//   -L/path/to/primitiv/libs     (typically -L../../build/primitiv)
//   mnist.cc -lprimitiv

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <primitiv/primitiv.h>

using primitiv::Device;
using primitiv::CPUDevice;
using primitiv::Graph;
using primitiv::Node;
using primitiv::Parameter;
using primitiv::trainers::SGD;
using primitiv::Shape;
using primitiv::initializers::Constant;
using primitiv::initializers::XavierUniform;
namespace F = primitiv::node_ops;
using namespace std;

namespace {

const unsigned NUM_TRAIN_SAMPLES = 60000;
const unsigned NUM_TEST_SAMPLES = 10000;
const unsigned NUM_INPUT_UNITS = 28 * 28;
const unsigned NUM_HIDDEN_UNITS = 800;
const unsigned NUM_OUTPUT_UNITS = 10;
const unsigned BATCH_SIZE = 200;
const unsigned NUM_TRAIN_BATCHES = NUM_TRAIN_SAMPLES / BATCH_SIZE;
const unsigned NUM_TEST_BATCHES = NUM_TEST_SAMPLES / BATCH_SIZE;
const unsigned MAX_EPOCH = 100;

// Helper function to load input images.
vector<float> load_images(const string &filename, const unsigned n) {
  ifstream ifs(filename, ios::binary);
  if (!ifs.is_open()) {
    cerr << "File could not be opened: " << filename << endl;
    abort();
  }

  ifs.ignore(16);  // header
  const unsigned size = n * NUM_INPUT_UNITS;
  vector<unsigned char> buf(size);
  ifs.read(reinterpret_cast<char *>(&buf[0]), size);
  vector<float> ret(size);
  for (unsigned i = 0; i < size; ++i) ret[i] = buf[i] / 255.0;
  return ret;
}

// Helper function to load labels.
vector<char> load_labels(const string &filename, const unsigned n) {
  ifstream ifs(filename, ios::binary);
  if (!ifs.is_open()) {
    cerr << "File could not be opened: " << filename << endl;
    abort();
  }

  ifs.ignore(8);  // header
  vector<char> ret(n);
  ifs.read(&ret[0], n);
  return ret;
}

}  // namespace

int main() {
  // Loads data
  vector<float> train_inputs = ::load_images("data/train-images-idx3-ubyte", NUM_TRAIN_SAMPLES);
  vector<char> train_labels = ::load_labels("data/train-labels-idx1-ubyte", NUM_TRAIN_SAMPLES);
  vector<float> test_inputs = ::load_images("data/t10k-images-idx3-ubyte", NUM_TEST_SAMPLES);
  vector<char> test_labels = ::load_labels("data/t10k-labels-idx1-ubyte", NUM_TEST_SAMPLES);

  // Uses CPU.
  CPUDevice dev;
  Device::set_default_device(dev);

  // Parameters for the multilayer perceptron.
  Parameter pw1("w1", {NUM_HIDDEN_UNITS, NUM_INPUT_UNITS}, XavierUniform());
  Parameter pb1("b1", {NUM_HIDDEN_UNITS}, Constant(0));
  Parameter pw2("w2", {NUM_OUTPUT_UNITS, NUM_HIDDEN_UNITS}, XavierUniform());
  Parameter pb2("b2", {NUM_OUTPUT_UNITS}, Constant(0));

  // Parameters for batch normalization.
  //Parameter pbeta("beta", {NUM_HIDDEN_UNITS}, Constant(0));
  //Parameter pgamma("gamma", {NUM_HIDDEN_UNITS}, Constant(1));

  // Trainer
  SGD trainer(.5);
  trainer.add_parameter(pw1);
  trainer.add_parameter(pb1);
  trainer.add_parameter(pw2);
  trainer.add_parameter(pb2);
  //trainer.add_parameter(&pbeta);
  //trainer.add_parameter(&pgamma);

  // Helper lambda to construct the predictor network.
  auto make_graph = [&](const vector<float> &inputs, bool train) {
    // Stores input values.
    Node x = F::input(Shape({NUM_INPUT_UNITS}, BATCH_SIZE), inputs);
    // Calculates the hidden layer.
    Node w1 = F::input(pw1);
    Node b1 = F::input(pb1);
    Node h = F::relu(F::matmul(w1, x) + b1);
    // Batch normalization
    //Node beta = F::input(pbeta);
    //Node gamma = F::input(pgamma);
    //h = F::batch::normalize(h) * gamma + beta;
    // Dropout
    h = F::dropout(h, .5, train);
    // Calculates the output layer.
    Node w2 = F::input(pw2);
    Node b2 = F::input(pb2);
    return F::matmul(w2, h) + b2;
  };

  // Batch randomizer
  mt19937 rng;
  vector<unsigned> ids(NUM_TRAIN_SAMPLES);
  iota(begin(ids), end(ids), 0);

  for (unsigned epoch = 0; epoch < MAX_EPOCH; ++epoch) {
    // Shuffles sample IDs.
    shuffle(begin(ids), end(ids), rng);

    // Training loop
    for (unsigned batch = 0; batch < NUM_TRAIN_BATCHES; ++batch) {
      // Makes a minibatch for training.
      vector<float> inputs(BATCH_SIZE * NUM_INPUT_UNITS);
      vector<unsigned> labels(BATCH_SIZE);
      for (unsigned i = 0; i < BATCH_SIZE; ++i) {
        const unsigned id = ids[i + batch * BATCH_SIZE];
        copy(&train_inputs[id * NUM_INPUT_UNITS],
             &train_inputs[(id + 1) * NUM_INPUT_UNITS],
             &inputs[i * NUM_INPUT_UNITS]);
        labels[i] = train_labels[id];
      }

      trainer.reset_gradients();
      {
        // Constructs the graph.
        Graph g;
        Graph::set_default_graph(g);
        Node y = make_graph(inputs, true);
        Node loss = F::softmax_cross_entropy(y, 0, labels);
        Node avg_loss = F::batch::mean(loss);

        // Dump computation graph at the first time.
        //if (epoch == 0 && batch == 0) g.dump();

        // Forward, backward, and updates parameters.
        g.forward(avg_loss);
        g.backward(avg_loss);
      }
      trainer.update(1);
    }

    unsigned match = 0;

    // Test loop
    for (unsigned batch = 0; batch < NUM_TEST_BATCHES; ++batch) {
      // Makes a test minibatch.
      vector<float> inputs(BATCH_SIZE * NUM_INPUT_UNITS);
      copy(&test_inputs[batch * BATCH_SIZE * NUM_INPUT_UNITS],
           &test_inputs[(batch + 1) * BATCH_SIZE * NUM_INPUT_UNITS],
           &inputs[0]);

      // Constructs the graph.
      Graph g;
      Graph::set_default_graph(g);
      Node y = make_graph(inputs, false);

      // Gets outputs, argmax, and compares them with the label.
      vector<float> y_val = g.forward(y).to_vector();
      for (unsigned i = 0; i < BATCH_SIZE; ++i) {
        float maxval = -1e10;
        unsigned argmax = -1;
        for (unsigned j = 0; j < NUM_OUTPUT_UNITS; ++j) {
          float v = y_val[j + i * NUM_OUTPUT_UNITS];
          if (v > maxval) maxval = v, argmax = j;
        }
        if (argmax == test_labels[i + batch * BATCH_SIZE]) ++match;
      }
    }

    const float accuracy = 100.0 * match / NUM_TEST_SAMPLES;
    printf("epoch %d: accuracy: %.2f%%\n", epoch, accuracy);
    //pw1.save("mnist-params-w1.yaml");
    //pb1.save("mnist-params-b1.yaml");
    //pw2.save("mnist-params-w2.yaml");
    //pb2.save("mnist-params-b2.yaml");
    //pbeta.save("mnist-params-beta.yaml");
    //pgamma.save("mnist-params-gamma.yaml");
    //cout << "epoch " << epoch << ": saved parameters." << endl;
  }

  return 0;
}
