// Sample code to train the RNNLM using preprocessed Penn Treebank dataset:
//   http://www.fit.vutbr.cz/~imikolov/rnnlm/simple-examples.tgz
//
// The model is based on Eq. (1) to (5) in following paper;
//   Mikolov et al., "Recurrent neural network based language model."
//   http://www.fit.vutbr.cz/research/groups/speech/publi/2010/mikolov_interspeech2010_IS100722.pdf
//
// Usage:
//   Run 'download_ptb.sh' in the same directory before using this code.
// g++
//   -std=c++11
//   -I/path/to/primitiv/includes (typically -I../..)
//   -L/path/to/primitiv/libs     (typically -L../../build/primitiv)
//   ptb_rnnlm.cc -lprimitiv

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <primitiv/primitiv.h>
#include <primitiv/primitiv_cuda.h>

using primitiv::initializers::XavierUniform;
using primitiv::trainers::Adam;
namespace F = primitiv::node_ops;
using namespace primitiv;
using namespace std;

namespace {

static const unsigned NUM_HIDDEN_UNITS = 256;
static const unsigned BATCH_SIZE = 64;
static const unsigned MAX_EPOCH = 100;

// Gathers the set of words from space-separated corpus.
unordered_map<string, unsigned> make_vocab(const string &filename) {
  ifstream ifs(filename);
  if (!ifs.is_open()) {
    cerr << "File could not be opened: " << filename << endl;
    exit(1);
  }
  unordered_map<string, unsigned> vocab;
  string line, word;
  unsigned lno = 0;
  while (getline(ifs, line)) {
    line = "<s>" + line + "<s>";
    stringstream ss(line);
    while (getline(ss, word, ' ')) {
      if (vocab.find(word) == vocab.end()) {
        const unsigned id = vocab.size();
        vocab.emplace(make_pair(word, id));
      }
    }
  }
  return vocab;
}

// Generates word ID list using corpus and vocab.
vector<vector<unsigned>> load_corpus(
    const string &filename, const unordered_map<string, unsigned> &vocab) {
  ifstream ifs(filename);
  if (!ifs.is_open()) {
    cerr << "File could not be opened: " << filename << endl;
    exit(1);
  }
  vector<vector<unsigned>> corpus;
  string line, word;
  while (getline(ifs, line)) {
    line = "<s>" + line + "<s>";
    stringstream ss (line);
    vector<unsigned> sentence;
    while (getline(ss, word, ' ')) {
      sentence.emplace_back(vocab.at(word));
    }
    corpus.emplace_back(move(sentence));
  }
  return corpus;
}

// Counts output labels in the corpus.
unsigned count_labels(const vector<vector<unsigned>> &corpus) {
  unsigned ret = 0;
  for (const auto &sent :corpus) ret += sent.size() - 1;
  return ret;
}

// Extracts a minibatch from loaded corpus
vector<vector<unsigned>> make_batch(
    const vector<vector<unsigned>> &corpus,
    const vector<unsigned> &sent_ids,
    unsigned eos_id) {
  const unsigned batch_size = sent_ids.size();
  unsigned max_len = 0;
  for (const unsigned sid : sent_ids) {
    max_len = std::max<unsigned>(max_len, corpus[sid].size());
  }
  vector<vector<unsigned>> batch(max_len, vector<unsigned>(batch_size, eos_id));
  for (unsigned i = 0; i < batch_size; ++i) {
    const auto &sent = corpus[sent_ids[i]];
    for (unsigned j = 0; j < sent.size(); ++j) {
      batch[j][i] = sent[j];
    }
  }
  return batch;
}

class RNNLM {
public:
  RNNLM(unsigned vocab_size, unsigned eos_id, Device &dev, Trainer &trainer)
    : dev_(dev)
    , eos_id_(eos_id)
    , pwlookup_("Lookup", {NUM_HIDDEN_UNITS, vocab_size}, XavierUniform(), &dev)
    , pwxs_("Wxs", {NUM_HIDDEN_UNITS, NUM_HIDDEN_UNITS}, XavierUniform(), &dev)
    , pwsy_("Wsy", {vocab_size, NUM_HIDDEN_UNITS}, XavierUniform(), &dev) {
      trainer.add_parameter(&pwlookup_);
      trainer.add_parameter(&pwxs_);
      trainer.add_parameter(&pwsy_);
    }

  // Forward function of RNNLM. Input data should be arranged below:
  // inputs = {
  //   {sent1_word1, sent2_word1, ..., sentN_word1},  // 1st input (<s>)
  //   {sent1_word2, sent2_word2, ..., sentN_word2},  // 2nd input/1st output
  //   ...,
  //   {sent1_wordM, sent2_wordM, ..., sentN_wordM},  // last output (<s>)
  // };
  vector<Node> forward(const vector<vector<unsigned>> &inputs, Graph &g) {
    const unsigned batch_size = inputs[0].size();
    Node wlookup = F::input(&pwlookup_, &g);
    Node wxs = F::input(&pwxs_, &g);
    Node wsy = F::input(&pwsy_, &g);
    Node s = F::zeros(Shape({NUM_HIDDEN_UNITS}, batch_size), &dev_, &g);
    vector<Node> outputs;
    for (unsigned i = 0; i < inputs.size() - 1; ++i) {
      Node w = F::pick(wlookup, 1, inputs[i]);
      Node x = w + s;
      Node s = F::sigmoid(F::matmul(wxs, x));
      outputs.emplace_back(F::matmul(wsy, s));
    }
    return outputs;
  }

  // Loss function.
  Node forward_loss(
      const vector<Node> &outputs, const vector<vector<unsigned>> &inputs) {
    vector<Node> losses;
    for (unsigned i = 0; i < outputs.size(); ++i) {
      losses.emplace_back(
          F::softmax_cross_entropy(outputs[i], 0, inputs[i + 1]));
    }
    return F::batch::mean(F::sum(losses));
  }

private:
  Device &dev_;
  unsigned eos_id_;
  Parameter pwlookup_;
  Parameter pwxs_;
  Parameter pwsy_;
};

}  // namespace

int main() {
  // Loads vocab.
  const auto vocab = ::make_vocab("data/ptb.train.txt");
  cout << "#vocab: " << vocab.size() << endl;  // maybe 10000
  const unsigned eos_id = vocab.at("<s>");

  // Loads all corpus.
  const auto train_corpus = ::load_corpus("data/ptb.train.txt", vocab);
  const auto valid_corpus = ::load_corpus("data/ptb.valid.txt", vocab);
  const unsigned num_train_sents = train_corpus.size();
  const unsigned num_valid_sents = valid_corpus.size();
  const unsigned num_train_labels = ::count_labels(train_corpus);
  const unsigned num_valid_labels = ::count_labels(valid_corpus);
  cout << "train: " << num_train_sents << " sentences, "
                    << num_train_labels << " labels" << endl;
  cout << "valid: " << num_valid_sents << " sentences, "
                    << num_valid_labels << " labels" << endl;

  // Uses GPU.
  CUDADevice dev(0);

  // Trainer.
  Adam trainer;

  // Our LM.
  ::RNNLM lm(vocab.size(), eos_id, dev, trainer);

  // Batch randomizer.
  random_device rd;
  mt19937 rng(rd());

  // Sentence IDs.
  vector<unsigned> train_ids(num_train_sents);
  vector<unsigned> valid_ids(num_valid_sents);
  iota(begin(train_ids), end(train_ids), 0);
  iota(begin(valid_ids), end(valid_ids), 0);

  // Train/valid loop.
  for (unsigned epoch = 0; epoch < MAX_EPOCH; ++epoch) {
    cout << "epoch " << (epoch + 1) << '/' << MAX_EPOCH << ':' << endl;
    // Shuffles train sentence IDs.
    shuffle(begin(train_ids), end(train_ids), rng);

    // Training.
    float train_loss = 0;
    for (unsigned ofs = 0; ofs < num_train_sents; ofs += BATCH_SIZE) {
      const vector<unsigned> batch_ids(
          begin(train_ids) + ofs,
          begin(train_ids) + std::min<unsigned>(
            ofs + BATCH_SIZE, num_train_sents));
      const auto batch = ::make_batch(train_corpus, batch_ids, eos_id);
      trainer.reset_gradients();
      {
        Graph g;
        const auto outputs = lm.forward(batch, g);
        const auto loss = lm.forward_loss(outputs, batch);
        train_loss += g.forward(loss).to_vector()[0] * batch_ids.size();
        g.backward(loss);
      }
      trainer.update(1);
      cout << ofs << '\r' << flush;
    }
    const float train_ppl = std::exp(train_loss / num_train_labels);
    cout << "  train ppl = " << train_ppl << endl;

    // Validation.
    float valid_loss = 0;
    for (unsigned ofs = 0; ofs < num_valid_sents; ofs += BATCH_SIZE) {
      const vector<unsigned> batch_ids(
          begin(valid_ids) + ofs,
          begin(valid_ids) + std::min<unsigned>(
            ofs + BATCH_SIZE, num_valid_sents));
      const auto batch = ::make_batch(valid_corpus, batch_ids, eos_id);
      Graph g;
      const auto outputs = lm.forward(batch, g);
      const auto loss = lm.forward_loss(outputs, batch);
      valid_loss += g.forward(loss).to_vector()[0] * batch_ids.size();
      cout << ofs << '\r' << flush;
    }
    const float valid_ppl = std::exp(valid_loss / num_valid_labels);
    cout << "  valid ppl = " << valid_ppl << endl;
  }

  return 0;
}
